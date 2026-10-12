// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"   // wndr 5.8: needed for check()/etc. used by ImageCore.h below (no shared PCH here)
#include "Containers/SharedString.h"
#include "ImageCore.h"
#include "DDSFile.h"
#include "Modules/ModuleManager.h"
#include "TextureCompressorModule.h"
#include "Interfaces/ITextureFormat.h"
#include "Interfaces/ITextureFormatModule.h"
#include "PixelFormat.h"
#include "Engine/TextureDefines.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/ScopeLock.h"
#include "Misc/SecureHash.h"
#include "Async/ParallelFor.h"
#include "Async/TaskGraphInterfaces.h"
#include "Misc/FileHelper.h"
#include "Runtime/Launch/Resources/Version.h"
#include "Serialization/CompactBinary.h"
#include "Serialization/CompactBinaryWriter.h"
#include "DerivedDataBuildFunctionFactory.h"
#include "Tasks/Task.h"
#include "TextureBuildFunction.h"
#include "HAL/FileManager.h"
#include "HAL/LowLevelMemTracker.h"
#include "Misc/WildcardString.h"
#include "Misc/CommandLine.h"

#if WITH_BASIS_COMPRESSOR
#pragma push_macro("check")
#pragma push_macro("verify")
#undef check
#undef verify
#include "encoder/basisu_enc.h"
#include "encoder/basisu_comp.h"
#include "transcoder/basisu_transcoder.h"
#include "encoder/basisu_gpu_texture.h"
#pragma pop_macro("verify")
#pragma pop_macro("check")
#endif // WITH_BASIS_COMPRESSOR

/**********

Basis Universal can do both RDO (rate distortion optimization) and non-RDO encoding to BC1-7, ETC1-2, and ASTC.

This is controlled using the project texture compression settings and the corresponding
Compress Speed.

The texture property Lossy Compression Amount is converted to a quality level to use.
This property can be adjusted via LODGroup or per texture. If not set in either place,
the project settings provide a default value.

=====================

TextureFormatBasis handles formats TFB_DXT1,etc.

Use of this format (instead of DXT1) is enabled with TextureFormatPrefix in config, such as :

\Engine\Config\BaseEngine.ini

[AlternateTextureCompression]
TextureCompressionFormat="TextureFormatBasis"
TextureFormatPrefix="TFB_"

When this is enabled, the formats like "DXT1" are renamed to "TFB_DXT1" and are handled by this encoder.

Basis Universal RDO encoding can be slow, but is cached in the DDC so should only be slow the first time.
A fast local network shared DDC is recommended.

RDO encoding and compression level can be enabled separately in the editor vs cooks using settings described
below.

========================


Basis Universal Settings
----------------------

TextureFormatBasis reads settings from Engine.ini ; they're created by default
when not found.  Note they are created in per-platform Engine.ini, you can
find them and move them up to DefaultEngine if you want them to be global.

The INI settings block looks like :

[TextureFormatBasisSettings]
bDebugColor=False
GlobalQualityMultiplier=1.0

The sense of the bools is set so that all-false is default behavior.

bDebugColor :

Fills the encoded texture with a solid color depending on their BCN format.
This is a handy way to see that you are in fact getting Basis Universal in your game.
It's also an easy way to spot textures that aren't BCN compressed, since they will not
be solid color.  (for example I found that lots of the Unreal demo content uses "HDR"
which is an uncompressed format, instead of "HDRCompressed" (BC6))  The color indicates
the actual compressed format output.

GlobalQualityMultiplier :

Takes all quality levels and scales them by this multiplier, so it affects the global default
and the per-texture quality levels.

It is recommended to leave this at 1.0 until you get near shipping your final game, at
which point you could tweak it to 0.9 or 1.1 to adjust your package size without having
to edit lots of per-texture quality levels.

Basis Universal quality
----------------------

The "quality" parameter is the most important way of controlling Basis Universal RDO.

"quality" controls the tradeoff of size vs quality in the Rate Distortion Optimization.

Finding the right quality settings will be a collaboration between artists and
programmers.  Programmers and technical artists may wish to find a global quality
that meets your goals.  Individual texture artists may wish to tweak the quality
per-texture when needed, but this should be rare - for the most part Basis Universal
quality is very predictable and good on most textures.

Quality first of all can be overridden per texture with the "LossyCompressionAmount"
setting.  This is a slider in the GUI in the editor that goes from Lowest to Highest.
The default value is "Default" and we recommend leaving that there most of the time.

If the per-texture LossyCompressionAmount is "Default", that means "inherit from LODGroup".

The LODGroup gives you a logical group of textures where you can adjust the quality on that
whole set of textures rather than per-texture.
When you're in the editor and you adjust the LossyCompressionAmount or import a 
new texture, it shows the Basis Universal encoded result in the texture preview.

*********/


#if WITH_BASIS_COMPRESSOR

DEFINE_LOG_CATEGORY_STATIC(LogTextureFormatBasis, Log, All);
LLM_DEFINE_TAG(BasisTexture);

struct FBasisTextureVTable;

static void TFB_Plugins_Init();
static void TFB_Plugins_Install(const FBasisTextureVTable* VTable);

/**
*
* FBasisTextureVTable provides function calls to a specific version of the Basis Texture dynamic lib
*   multiple FBasisTextureVTables may be loaded to support multi-version encoding
*
**/
struct FBasisTextureVTable
{
	const TCHAR* VersionString = nullptr;
	FName	Version;

	// LoadedDynamicLib is set on first use
	//	if either of LoadedDynamicLib or LoadFailed is set, a load was attempted, don't try again
	//	if both == 0, load has not been tried yet
	void* LoadedDynamicLib = nullptr;
	std::atomic<int> LoadResult = 0; // 0 = not done, 1 = ok, -1 = fail
	FCriticalSection DynamicLibLoadLock;

	FBasisTextureVTable()
	{
	}

	void Init(const TCHAR* InVersionString)
	{
		// this runs from Module init, threads are not running
		VersionString = InVersionString;
		Version = FName(InVersionString);
	}

	bool TryLoad()
	{
		// load DLL on demand
		// this can run some threads so must be thread safe

		int GotLoadResult = LoadResult.load(std::memory_order_acquire);
		if (GotLoadResult)
		{
			return (GotLoadResult > 0);
		}

		// else try to load :

		// Lock so only one thread does init :
		FScopeLock TryLoadLock(&DynamicLibLoadLock);

		// double check inside lock :

		GotLoadResult = LoadResult.load(std::memory_order_acquire);
		if (GotLoadResult)
		{
			return (GotLoadResult > 0);
		}

		// TFB_DLL_PREFIX/SUFFIX is set by the build.cs with the right names for this platform
		FString DynamicLibName = FString(TFB_DLL_PREFIX) + VersionString + FString(TFB_DLL_SUFFIX);

		// I want to see this log by default in Cook+Editor , but not in TBW
#ifndef VerboseIfNotEditor
#if WITH_EDITOR
#define VerboseIfNotEditor	Display
#else
#define VerboseIfNotEditor	Verbose
#endif
#endif

		UE_LOG(LogTextureFormatBasis, VerboseIfNotEditor, TEXT("Basis Texture loading DLL: %s"), *DynamicLibName);

		void* DynamicLib = FPlatformProcess::GetDllHandle(*DynamicLibName);
		if (DynamicLib == nullptr)
		{
			UE_LOG(LogTextureFormatBasis, Warning, TEXT("Basis Texture %s requested but could not be loaded"), *DynamicLibName);

			LoadResult.store(-1, std::memory_order_release); // publish
			return false;
		}

		// TODO: load basisu functions

		TFB_Plugins_Install(this);

		LoadedDynamicLib = DynamicLib;
		LoadResult.store(1, std::memory_order_release); // publish

		return true;
	}

	~FBasisTextureVTable()
	{
		if (LoadedDynamicLib)
		{
			FPlatformProcess::FreeDllHandle(LoadedDynamicLib);
			LoadedDynamicLib = nullptr;
		}
	}
};


class FBasisTextureBuildFunction final : public FTextureBuildFunction
{
	const UE::FUtf8SharedString& GetName() const final
	{
		static const UE::FUtf8SharedString Name(UTF8TEXTVIEW("BasisTexture"));
		return Name;
	}

	void GetVersion(UE::DerivedData::FBuildVersionBuilder& Builder, ITextureFormat*& OutTextureFormatVersioning) const final
	{
		static FGuid Version(TEXT("e6b8884f-923a-44a1-8da1-298fb48865b2"));
		Builder << Version;
		OutTextureFormatVersioning = FModuleManager::GetModuleChecked<ITextureFormatModule>(TEXT("TextureFormatBasis")).GetTextureFormat();
	}
};



struct FBasisFormatMapping
{
	EPixelFormat PixelFormat;
	basist::transcoder_texture_format BasisFormat;
};

static FBasisFormatMapping GFormatMap[] =
{
	{ PF_DXT1, basist::transcoder_texture_format::cTFBC1_RGB },
	{ PF_DXT3, basist::transcoder_texture_format::cTFBC3_RGBA },
	{ PF_DXT5, basist::transcoder_texture_format::cTFBC3_RGBA },
	{ PF_BC4, basist::transcoder_texture_format::cTFBC4_R },
	{ PF_BC5, basist::transcoder_texture_format::cTFBC5_RG },
	{ PF_BC6H, basist::transcoder_texture_format::cTFBC6H },
	{ PF_BC7, basist::transcoder_texture_format::cTFBC7_RGBA },
	{ PF_ETC1, basist::transcoder_texture_format::cTFETC1_RGB },
	{ PF_ETC2_RGB, basist::transcoder_texture_format::cTFETC1_RGB },
	{ PF_ETC2_RGBA, basist::transcoder_texture_format::cTFETC2_RGBA },
	{ PF_ASTC_4x4, basist::transcoder_texture_format::cTFASTC_4x4_RGBA },
	{ PF_ASTC_6x6, basist::transcoder_texture_format::cTFASTC_HDR_6x6_RGBA },
};

static basist::transcoder_texture_format BasisFormatFromPixelFormat(EPixelFormat InFormat)
{
	for (const auto& Mapping : GFormatMap)
	{
		if (Mapping.PixelFormat == InFormat)
		{
			return Mapping.BasisFormat;
		}
	}
	return basist::transcoder_texture_format::cTFTotalTextureFormats; // Invalid
}

static EPixelFormat PixelFormatFromBasisFormat(basist::transcoder_texture_format InFormat)
{
	for (const auto& Mapping : GFormatMap)
	{
		if (Mapping.BasisFormat == InFormat)
		{
			return Mapping.PixelFormat;
		}
	}
	return PF_Unknown;
}

// user data passed to Basis Jobify system
static int BasisJobifyNumThreads = 0;
static void *BasisJobifyUserPointer = nullptr;
static bool BasisJobifyUseExampleJobify = false;

// enable this to make the DDC key unique (per build) for testing
//#define DO_FORCE_UNIQUE_DDC_KEY_PER_BUILD

#define ENUSUPPORTED_FORMATS(op) \
    op(DXT1) \
    op(DXT3) \
    op(DXT5) \
    op(DXT5n) \
    op(AutoDXT) \
    op(BC4) \
    op(BC5) \
	op(BC7) \
	op(ETC1) \
	op(ETC2_RGB) \
	op(ETC2_RGBA) \
	op(AutoETC2) \
	op(ASTC_4x4) \
	op(ASTC_6x6) \
	op(ASTC_RGB) \
	op(ASTC_RGBA) \
	op(ASTC_RGBAuto) \
	op(ASTC_RGB_HDR) \
	op(ASTC_4x4_NormalRG) \
	op(ASTC_6x6_NormalRG)

// register support for TFB_ prefixed names like "TFB_DXT1"
#define TEXTURE_FORMAT_PREFIX	"TFB_"
#define DECL_FORMAT_NAME(FormatName) static FName GTextureFormatName##FormatName = FName(TEXT(TEXTURE_FORMAT_PREFIX #FormatName));

ENUSUPPORTED_FORMATS(DECL_FORMAT_NAME);
#undef DECL_FORMAT_NAME

#define DECL_FORMAT_NAME_ENTRY(FormatName) GTextureFormatName##FormatName ,
static FName GSupportedTextureFormatNames[] =
{
	ENUSUPPORTED_FORMATS(DECL_FORMAT_NAME_ENTRY)
};
#undef DECL_FORMAT_NAME_ENTRY
#undef ENUSUPPORTED_FORMATS

#define BasisTextureVersion TEXT("1.60")

class FTextureFormatBasisConfig
{
public:
	struct FLocalDebugConfig
	{
		FLocalDebugConfig() :
			LogVerbosity(0)
		{
		}

		FString DebugDumpFilter; // dump textures that were encoded
		int LogVerbosity; // 0-2 ; 0=never, 1=large only, 2=always
	};

	FTextureFormatBasisConfig() :
		bDebugColor(false),
		GlobalQualityMultiplier(1.f)
	{
	}

	const FLocalDebugConfig& GetLocalDebugConfig() const
	{
		return LocalDebugConfig;
	}	

	void ImportFromConfigCache()
	{
		const TCHAR* IniSection = TEXT("TextureFormatBasisSettings");
		
		//
		// Note that while this gets called during singleton init for the module,
		// the INIs don't exist when we're being run as a texture build worker,
		// so all of these GConfig calls do nothing.
		// 
		
		// Class config variables
		GConfig->GetBool(IniSection, TEXT("bDebugColor"), bDebugColor, GEngineIni);
		GConfig->GetString(IniSection, TEXT("DebugDumpFilter"), LocalDebugConfig.DebugDumpFilter, GEngineIni);
		GConfig->GetInt(IniSection, TEXT("LogVerbosity"), LocalDebugConfig.LogVerbosity, GEngineIni);
		GConfig->GetFloat(IniSection, TEXT("GlobalQualityMultiplier"), GlobalQualityMultiplier, GEngineIni);
		
		FString CmdLineString;
		if (FParse::Value(FCommandLine::Get(), TEXT("-BasisDebugDumpFilter="), CmdLineString) )
		{
			UE_LOG(LogTextureFormatBasis, Display, TEXT("Enabling debug dump from command line: -BasisDebugDumpFilter=%s"), *CmdLineString);
			LocalDebugConfig.DebugDumpFilter = CmdLineString;
		}

		if (FParse::Param(FCommandLine::Get(), TEXT("BasisDebugColor")))
		{
			UE_LOG(LogTextureFormatBasis, Display, TEXT("Enabling debug color encoding from command line (-BasisDebugColor)"));
			bDebugColor = true;
		}

		// sanitize config values :
		if ( GlobalQualityMultiplier <= 0.f )
		{
			GlobalQualityMultiplier = 1.f;
		}

		//UE_LOG(LogTextureFormatBasis,VerboseIfNotEditor, TEXT("Basis Texture TFB init; latest sdk version = %s"),TEXT(BasisTextureVersion));
		#ifdef DO_FORCE_UNIQUE_DDC_KEY_PER_BUILD
		UE_LOG(LogTextureFormatBasis, Display, TEXT("Basis Texture DO_FORCE_UNIQUE_DDC_KEY_PER_BUILD"));
		#endif
	}

	FCbObject ExportToCb(const FTextureBuildSettings& BuildSettings) const
	{
		//
		// Here we write config stuff to the packet that gets sent to the build
		// workers.
		//
		// This is only for stuff that isn't already part of the build settings.
		//

		FCbWriter Writer;
		Writer.BeginObject("TextureFormatBasisSettings");

		if (bDebugColor)
		{
			Writer.AddBool("bDebugColor", bDebugColor);
		}
		if (GlobalQualityMultiplier != 1.f)
		{
			Writer.AddFloat("GlobalQualityMultiplier", GlobalQualityMultiplier);
		}

		Writer.EndObject();

		return Writer.Save().AsObject();
	}

	void GetBasisCompressParameters(EPixelFormat* OutCompressedPixelFormat, int* OutEffortLevel, bool* bOutDebugColor, const struct FTextureBuildSettings& InBuildSettings, bool bHasAlpha, bool& isNormalMap) const
	{
		//TRACE_CPUPROFILER_EVENT_SCOPE(Texture.GetBasisCompressParameters);

		FName TextureFormatName = InBuildSettings.BaseTextureFormatName;

		EPixelFormat CompressedPixelFormat = PF_Unknown;
		if (TextureFormatName == GTextureFormatNameDXT1)
		{
			CompressedPixelFormat = PF_DXT1;
		}
		else if (TextureFormatName == GTextureFormatNameDXT3)
		{
			CompressedPixelFormat = PF_DXT3;
		}
		else if (TextureFormatName == GTextureFormatNameDXT5)
		{
			CompressedPixelFormat = PF_DXT5;
		}
		else if (TextureFormatName == GTextureFormatNameAutoDXT)
		{
			CompressedPixelFormat = bHasAlpha ? PF_DXT5 : PF_DXT1;
		}
		else if (TextureFormatName == GTextureFormatNameDXT5n)
		{
			// Unreal already has global UseDXT5NormalMap config option
			// EngineSettings.GetString(TEXT("SystemSettings"), TEXT("Compat.UseDXT5NormalMaps")
			//	if that is false (which is the default) they use BC5
			// so this should be rarely use
			// (we prefer BC5 over DXT5n)
			CompressedPixelFormat = PF_DXT5;
			isNormalMap = true;
		}
		else if (TextureFormatName == GTextureFormatNameBC4)
		{
			CompressedPixelFormat = PF_BC4;
		}
		else if (TextureFormatName == GTextureFormatNameBC5)
		{
			CompressedPixelFormat = PF_BC5;
			isNormalMap = true;
		}
		else if (TextureFormatName == GTextureFormatNameBC7)
		{
			CompressedPixelFormat = PF_BC7;
		}
		else if (TextureFormatName == GTextureFormatNameETC1)
		{
			CompressedPixelFormat = PF_ETC1;
		}
		else if (TextureFormatName == GTextureFormatNameETC2_RGB)
		{
			CompressedPixelFormat = PF_ETC2_RGB;
		}
		else if (TextureFormatName == GTextureFormatNameETC2_RGBA)
		{
			CompressedPixelFormat = PF_ETC2_RGBA;
		}
		else if (TextureFormatName == GTextureFormatNameAutoETC2) {
			CompressedPixelFormat = bHasAlpha ? PF_ETC2_RGBA : PF_ETC2_RGB;
		}
		else if (TextureFormatName == GTextureFormatNameASTC_RGB) {
			CompressedPixelFormat = PF_ASTC_6x6;
		}
		else if (TextureFormatName == GTextureFormatNameASTC_RGBA) {
			CompressedPixelFormat = PF_ASTC_6x6;
		}
		else if (TextureFormatName == GTextureFormatNameASTC_RGBAuto) {
			CompressedPixelFormat = PF_ASTC_6x6;
		}
		else if (TextureFormatName == GTextureFormatNameASTC_4x4)
		{
			CompressedPixelFormat = PF_ASTC_4x4;
		}
		else if (TextureFormatName == GTextureFormatNameASTC_6x6)
		{
			CompressedPixelFormat = PF_ASTC_6x6;
		}
		else if (TextureFormatName == GTextureFormatNameASTC_RGB_HDR)
		{
			CompressedPixelFormat = PF_ASTC_6x6_HDR;
		}
		else if (TextureFormatName == GTextureFormatNameASTC_4x4_NormalRG)
		{
			CompressedPixelFormat = PF_ASTC_4x4;
			isNormalMap = true;
		}
		else if (TextureFormatName == GTextureFormatNameASTC_6x6_NormalRG)
		{
			CompressedPixelFormat = PF_ASTC_6x6;
			isNormalMap = true;
		}
		else
		{
			UE_LOG(LogTextureFormatBasis,Fatal, TEXT("Unsupported TextureFormatName for compression: %s"), *TextureFormatName.ToString());
		}
		
		*OutCompressedPixelFormat = CompressedPixelFormat;

		// Use the DDC2 provided value if it exists.
		bool bUseDebugColor = InBuildSettings.FormatConfigOverride.FindView("bDebugColor").AsBool(bDebugColor);

		float UseGlobalQualityMultiplier = InBuildSettings.FormatConfigOverride.FindView("GlobalQualityMultiplier").AsFloat(GlobalQualityMultiplier);

		// EffortLevel might be set to faster modes for previewing vs cooking or something
		//	but I don't see people setting that per-Texture or in lod groups or any of that
		//  it's more about cook mode (fast vs final bake)	

		// Note InBuildSettings.OodleEncodeEffort is an ETextureEncodeEffort
		// we map it to Basis compression_level
		typedef enum Basis_EncodeEffortLevel
		{
			BasisTex_EncodeEffortLevel_Default = 0,
			BasisTex_EncodeEffortLevel_Low = 10,
			BasisTex_EncodeEffortLevel_Normal = 20,
			BasisTex_EncodeEffortLevel_High = 30,
		} BasisTex_EncodeEffortLevel;
		int EffortLevel = 0;
		switch ((Basis_EncodeEffortLevel)InBuildSettings.OodleEncodeEffort)
		{
		case BasisTex_EncodeEffortLevel_Low:
			EffortLevel = 1;
			break;
		case BasisTex_EncodeEffortLevel_Normal:
			EffortLevel = 2;
			break;
		case BasisTex_EncodeEffortLevel_High:
			EffortLevel = 3;
			break;
		case BasisTex_EncodeEffortLevel_Default:
		default:
			EffortLevel = 2;
			break;
		}

		*bOutDebugColor = bUseDebugColor;
		*OutEffortLevel = EffortLevel;
	}

private:
	// the sense of these bools is set so that default behavior = all false
	bool bDebugColor; // color textures by their BCN, for data discovery
	// after quality is set, multiply by this scale factor :
	//	(multiplies the default and per-Texture overrides)
	//	is intended to let you do last minute whole-game adjustment
	float GlobalQualityMultiplier;
	FLocalDebugConfig LocalDebugConfig;
};

class FTextureFormatBasis : public ITextureFormat
{
public:

	FTextureFormatBasisConfig GlobalFormatConfig;
	TArray<FBasisTextureVTable> VTables;
	FName BasisTextureVersionLatest;
	FName BasisTextureSdkVersionToUseIfNone;

	FTextureFormatBasis() :
		BasisTextureVersionLatest(BasisTextureVersion),
		BasisTextureSdkVersionToUseIfNone("1.60")
	{
		// BasisTextureSdkVersionToUseIfNone is the fallback version to use if none is in the Texture uasset
		//	and also no remap pref is set
		// it should not be latest; it should be oldest (1.60)
		//  BasisTextureSdkVersionToUseIfNone should never be changed
		// if you want to map none to a newer version use config ini option AlternateTextureCompression/BasisTextureSdkVersionToUseIfNone
	}

	virtual ~FTextureFormatBasis()
	{
	}
	
	virtual bool AllowParallelBuild() const override
	{
		return true;
	}

	virtual bool SupportsEncodeSpeed(FName Format, const ITargetPlatformSettings* TargetPlatform) const override
	{
		return true;
	}

	virtual FName GetEncoderName(FName Format) const override
	{
		static const FName BasisName("UE5 Basis Universal Texture");
		return BasisName;
	}
	
	virtual FCbObject ExportGlobalFormatConfig(const FTextureBuildSettings& BuildSettings) const override
	{
		return GlobalFormatConfig.ExportToCb(BuildSettings);
	}

	bool Init()
	{
		TFB_Plugins_Init();

		// this is done at Singleton init time, the first time GetTextureFormat() is called
		GlobalFormatConfig.ImportFromConfigCache();

		// load ALL Basis DLL versions we support :
		// !! add new versions of Basis here !!

		const TCHAR * BasisTextureVersions[] =
		{
			TEXT("1.60"),
		};
		const int32 BasisTextureVersionsCount = (int32)( sizeof(BasisTextureVersions)/sizeof(BasisTextureVersions[0]) );

		VTables.SetNum(BasisTextureVersionsCount);

		// set up the VTable versions but don't actually load them yet
		// they will be loaded on first use
		for(int32 i=0;i<BasisTextureVersionsCount;i++)
		{
			VTables[i].Init( BasisTextureVersions[i] );
		}

		basisu::basisu_encoder_init(false, false);

		return true;
	}
	
	const FBasisTextureVTable * GetBasisTextureVTable(const FName & InVersion) const
	{
		for(int i=0;i<VTables.Num();i++)
		{
			if ( VTables[i].Version == InVersion )
			{
				// before we return the VTable pointer, make sure it is loaded :
				if ( ! const_cast<FBasisTextureVTable &>(VTables[i]).TryLoad() )
					return nullptr;

				return &(VTables[i]);
			}
		}

		return nullptr;
	}
	
	// increment this to invalidate Derived Data Cache to recompress everything
	#define DDC_BASIS_TEXTURE_VERSION 1

	virtual uint16 GetVersion(FName Format, const FTextureBuildSettings* InBuildSettings) const override
	{
		// note: InBuildSettings == NULL is used by GetVersionFormatNumbersForIniVersionStrings
		//	just to get a displayable version number

		return DDC_BASIS_TEXTURE_VERSION; 
	}
	
	virtual FString GetAlternateTextureFormatPrefix() const override
	{
		static const FString Prefix(TEXT(TEXTURE_FORMAT_PREFIX));
		return Prefix;
	}

	virtual FName GetLatestSdkVersion() const override
	{
		return BasisTextureVersionLatest;
	}

	virtual FString GetDerivedDataKeyString(const FTextureBuildSettings& InBuildSettings, int32 InMipCount, const FIntVector3& InMip0Dimensions) const override
	{
		// return all parameters that affect our output Texture
		// so if any of them change, we rebuild

		int Quality;
		int EffortLevel;
		EPixelFormat CompressedPixelFormat;
		bool bDebugColor;

		// @todo Basis : use InBuildSettings.GetOutputAlphaFromKnownAlphaOrFallback() instead (but that could change DDC keys)
		bool bHasAlpha = !InBuildSettings.bForceNoAlphaChannel;
		bool bIsNormalMap = false;

		switch (InBuildSettings.LossyCompressionAmount)
		{
		default:
		case TLCA_Default: Quality = 128; break;         // "Default"
		case TLCA_None:    Quality = 255; break;    // "No lossy compression"
		case TLCA_Lowest:  Quality = 128; break;    // "Lowest (Best Image quality, largest filesize)"
		case TLCA_Low:     Quality = 96; break;   // "Low"
		case TLCA_Medium:  Quality = 82; break;   // "Medium"
		case TLCA_High:    Quality = 64; break;   // "High"
		case TLCA_Highest: Quality = 32; break;   // "Highest (Worst Image quality, smallest filesize)"
		}

		GlobalFormatConfig.GetBasisCompressParameters(&CompressedPixelFormat, &EffortLevel, &bDebugColor, InBuildSettings, bHasAlpha, bIsNormalMap);

		int icpf = (int)CompressedPixelFormat;
		int Seed = 3;

		check(Quality < 256);
		if (bDebugColor)
		{
			// Debug Color is solid or check for RDO/ no RDO
			//	so make different DDC keys :
			if (Quality == 0)
				Quality = 256;
			else
				Quality = 257;
			EffortLevel = 1; // Default
		}

		FString DDCString = FString::Printf(TEXT("Basis_CPF%d_Q%d_E%d%d"), icpf, (int)Quality, (int)EffortLevel, Seed);

		// BasisTextureSdkVersion was added ; keys where BasisTextureSdkVersion is none are unchanged
		if (!InBuildSettings.OodleTextureSdkVersion.IsNone()) // Repurposing OodleTextureSdkVersion
		{
			DDCString += TEXT("_V");

			// concatenate VersionString without . characters which are illegal in DDC
			// version is something like "1.60" , we'll add something like "_V160"
			FName UseBasisTextureSdkVersion = ValidateBasisTextureSdkVersion(InBuildSettings.OodleTextureSdkVersion); // Repurposing OodleTextureSdkVersion

			FString VersionString = UseBasisTextureSdkVersion.ToString();
			for (int32 i = 0; i < VersionString.Len(); i++)
			{
				if (VersionString[i] != TEXT('.'))
				{
					DDCString += VersionString[i];
				}
			}
		}

		#ifdef DO_FORCE_UNIQUE_DDC_KEY_PER_BUILD
		DDCString += TEXT(__DATE__);
		DDCString += TEXT(__TIME__);
		#endif

		return DDCString;
	}

	FName ValidateBasisTextureSdkVersion(FName DesiredBasisTextureSdkVersion) const
	{
		const FBasisTextureVTable * VTable = GetBasisTextureVTable(DesiredBasisTextureSdkVersion);
		if (VTable == nullptr)
		{
			// This gets called every time a key gets generated, which is every time IsCachedForPlatformData gets called by
			// the cooker. It gets out of hand quickly, and we don't really need to see it more than once for each one that's
			// wrong. This technically means that in the editor, if they hit it twice rebuilding separate things they'll only
			// see the warning once. Hopefully that's good enough. This should be a rare thing.
			static UE::FMutex BadVersionLock;
			static TSet<FName> SeenBadVersion;

			bool bAlreadySeen = false;
			
			BadVersionLock.Lock();
			SeenBadVersion.Add(DesiredBasisTextureSdkVersion, &bAlreadySeen);
			BadVersionLock.Unlock();
			
			if (!bAlreadySeen)
			{
				UE_LOG(LogTextureFormatBasis,Display,
					TEXT("Unsupported BasisTextureSdkVersion: %s ; instead using: %s"),
					*DesiredBasisTextureSdkVersion.ToString(),
					*BasisTextureVersionLatest.ToString()
					);
			}
			
			return BasisTextureVersionLatest;
		}
		else
		{
			return DesiredBasisTextureSdkVersion;
		}
	}

	virtual void GetSupportedFormats(TArray<FName>& OutFormats) const override
	{
		OutFormats.Append(GSupportedTextureFormatNames, sizeof(GSupportedTextureFormatNames)/sizeof(GSupportedTextureFormatNames[0]) ); 
	}

	virtual EPixelFormat GetEncodedPixelFormat(const FTextureBuildSettings& InBuildSettings, bool bImageHasAlphaChannel) const
	{
		int EffortLevel;
		EPixelFormat CompressedPixelFormat;
		bool bDebugColor;
		bool bIsNormalMap;

		GlobalFormatConfig.GetBasisCompressParameters(&CompressedPixelFormat, &EffortLevel, &bDebugColor, InBuildSettings, bImageHasAlphaChannel, bIsNormalMap);
		return CompressedPixelFormat;
	}

	static void DebugDumpDDS(const FStringView & DebugTexturePathName,
			int32 SizeX,int32 SizeY,int32 Slice, UE::DDS::EDXGIFormat DebugFormat, const TCHAR * InOrOut,
			const void * PixelData, size_t PixelDataSize)
	{
		if (DebugFormat != UE::DDS::EDXGIFormat::UNKNOWN)
		{
			UE::DDS::FDDSFile* DDS = UE::DDS::FDDSFile::CreateEmpty2D(SizeX, SizeY, 1, DebugFormat, UE::DDS::FDDSFile::CREATE_FLAG_NONE);

			if ( DDS->Mips[0].DataSize != PixelDataSize )
			{
				UE_LOG(LogTextureFormatBasis, Warning, TEXT("DebugDump mip sizes don't match %dx%d: %lld != %lld"), SizeX,SizeY, DDS->Mips[0].DataSize, PixelDataSize);
			}
			size_t MipCopySize = FMath::Min(PixelDataSize,(size_t)DDS->Mips[0].DataSize);
			FMemory::Memcpy(DDS->Mips[0].Data, PixelData, MipCopySize);

			const TCHAR * FormatStr = DXGIFormatGetName(DebugFormat);
			FString FileName = FString::Printf(TEXT("%.*s_%s_%dx%d_S%d_%s.dds"), DebugTexturePathName.Len(), DebugTexturePathName.GetData(), FormatStr, SizeX, SizeY, Slice, InOrOut);

			// Object paths a) can contain slashes as its a path, and we dont want a hierarchy and b) can have random characters we don't want
			FileName = FPaths::MakeValidFileName(FileName, TEXT('_'));

			// limit file name len
			// full path will still likely be longer than _MAX_PATH which breaks many programs
			if ( FileName.Len() >= 256 )
			{
				FileName = FileName.Right(255);
			}

			FileName = FPaths::ProjectSavedDir() + TEXT("OodleDebugImages/") + FileName;
				
			FArchive* Ar = IFileManager::Get().CreateFileWriter(*FileName);
			if (Ar != nullptr)
			{
				TArray64<uint8> DdsBytes;
				if (DDS->WriteDDS(DdsBytes) == UE::DDS::EDDSError::OK)
				{
					Ar->Serialize(DdsBytes.GetData(), DdsBytes.Num());
				}
				Ar->Close();
				delete Ar;
			}
			else
			{
				UE_LOG(LogTextureFormatBasis, Error, TEXT("Failed to open DDS debug file: %s"), *FileName);
			}

			delete DDS;
		}
	}

	bool CanAcceptNonF32Source(FName Format) const override
	{
		return true;
	}


	virtual bool CanDecodeFormat(EPixelFormat InPixelFormat) const
	{
		return false;
	}

	virtual bool DecodeImage(int32 InSizeX, int32 InSizeY, int32 InNumSlices, EPixelFormat InPixelFormat, bool bInSRGB, const FName& InTextureFormatName, FSharedBuffer InEncodedData, FImage& OutImage, FStringView InTextureName) const
	{
		return false;
	}

	virtual bool CompressImage(const FImage& InImage, const FTextureBuildSettings& InBuildSettings, const FIntVector3& InMip0Dimensions,
		int32 InMip0NumSlicesNoDepth, int32 InMipIndex, int32 InMipCount, FStringView DebugTexturePathName, const bool bInHasAlpha, FCompressedImage2D& OutImage) const override
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(TFBasis.CompressImage);

		check(InImage.SizeX > 0);
		check(InImage.SizeY > 0);
		check(InImage.NumSlices > 0);

		int Quality;
		int EffortLevel;
		EPixelFormat CompressedPixelFormat;
		bool bDebugColor;
		bool bIsNormalMap = false;

		GlobalFormatConfig.GetBasisCompressParameters(&CompressedPixelFormat, &EffortLevel, &bDebugColor, InBuildSettings, bInHasAlpha, bIsNormalMap);

		switch (InBuildSettings.LossyCompressionAmount)
		{
		default:
		case TLCA_Default: Quality = 128; break;         // "Default"
		case TLCA_None:    Quality = 255; break;    // "No lossy compression"
		case TLCA_Lowest:  Quality = 128; break;    // "Lowest (Best Image quality, largest filesize)"
		case TLCA_Low:     Quality = 96; break;   // "Low"
		case TLCA_Medium:  Quality = 82; break;   // "Medium"
		case TLCA_High:    Quality = 64; break;   // "High"
		case TLCA_Highest: Quality = 32; break;   // "Highest (Worst Image quality, smallest filesize)"
		}

		bool bIsHDR = (InImage.Format == ERawImageFormat::RGBA16F || InImage.Format == ERawImageFormat::RGBA32F) &&
			(CompressedPixelFormat == PF_BC6H || CompressedPixelFormat == PF_ASTC_6x6_HDR);
		if (CompressedPixelFormat == PF_BC6H)
		{
			bIsHDR = true;
		}

		basisu::basis_compressor_params params;

		if (bIsHDR)
		{
			FImage ImageCopyRGBA32F;
			const FImage* SourceImage = &InImage;
			if (InImage.Format != ERawImageFormat::RGBA32F)
			{
				InImage.CopyTo(ImageCopyRGBA32F, ERawImageFormat::RGBA32F, InBuildSettings.GetDestGammaSpace());
				SourceImage = &ImageCopyRGBA32F;
			}

			params.m_source_images.resize(SourceImage->NumSlices);
			for (int32 Slice = 0; Slice < SourceImage->NumSlices; ++Slice)
			{
				const uint8* SliceBytes = SourceImage->RawData.GetData() + ((int64)SourceImage->SizeX * SourceImage->SizeY * 16 * Slice);
				params.m_source_images[Slice].init((const uint8_t*)SliceBytes, SourceImage->SizeX, SourceImage->SizeY, 4);
			}
			params.m_hdr = true;
		}
		else
		{
			// Convert source to BGRA8
			FImage ImageCopyBGRA;
			const FImage* SourceImageBGRA = &InImage;
			if (InImage.Format != ERawImageFormat::BGRA8)
			{
				InImage.CopyTo(ImageCopyBGRA, ERawImageFormat::BGRA8, InBuildSettings.GetDestGammaSpace());
				SourceImageBGRA = &ImageCopyBGRA;
			}
			if (!bIsHDR)
			{
				params.m_swizzle[0] = 2;
				params.m_swizzle[1] = 1;
				params.m_swizzle[2] = 0;
				params.m_swizzle[3] = 3;
			}

			params.m_source_images.resize(SourceImageBGRA->NumSlices);
			for (int32 Slice = 0; Slice < SourceImageBGRA->NumSlices; ++Slice)
			{
				const uint8* SliceData = SourceImageBGRA->RawData.GetData() + ((int64)SourceImageBGRA->SizeX * SourceImageBGRA->SizeY * 4 * Slice);
				params.m_source_images[Slice].init(SliceData, SourceImageBGRA->SizeX, SourceImageBGRA->SizeY, 4);
			}
		}

		bool bUseUASTC = false;
		if (bUseUASTC) {
			// Scale the lambda quality level for UASTC (100 - 25000)
			float Lambda = 100.0 + (Quality - 1) * (25000.0 - 100.0) / (255.0 - 1.0);
			params.m_uastc = true;
			params.m_astc_hdr_6x6_options.m_lambda = Lambda; // Between 100 and 25000

			// Scale the 4x4 RDO quality level for UASTC (.25 - 10)
			float UASTCQuality = 0.25f + (Quality - 1) * (10.0f - 0.25f) / (255.0f - 1.0f);

			// Also set UASTC LDR 4x4's lambda
			params.m_rdo_uastc_ldr_4x4_quality_scalar = UASTCQuality; // Between 100 and 25000
			params.m_rdo_uastc_ldr_4x4 = true;
		}
		else {
			params.m_uastc = false;
			params.m_quality_level = Quality; // Between 1 and 255.
		}

		params.m_etc1s_compression_level = EffortLevel;
		params.m_multithreading = false;
		params.m_status_output = false;
		params.m_check_for_alpha = bInHasAlpha;

		if (bIsNormalMap)
		{
			params.m_perceptual = false;
			params.m_mip_srgb = false;
			params.m_no_selector_rdo = true;
			params.m_no_endpoint_rdo = true;
			if (!bIsHDR)
			{
				params.m_swizzle[0] = 2;
				params.m_swizzle[1] = 3;
				params.m_swizzle[2] = 0;
				params.m_swizzle[3] = 1;
			}
		}
		else
		{
			bool bIsSRGB = InBuildSettings.GetDestGammaSpace() == EGammaSpace::sRGB;
			params.m_perceptual = bIsSRGB;
			params.m_mip_srgb = bIsSRGB;
		}

		// Set up Basis format
		params.m_tex_type = basist::cBASISTexType2D;
		if (InBuildSettings.bCubemap)
		{
			check(InImage.NumSlices % 6 == 0);
			params.m_tex_type = basist::cBASISTexTypeCubemapArray;
		}
		else if (InImage.NumSlices > 1)
		{
			// Could be 2D array, volume.
			// For now, assume 2D Array for simplicity.
			params.m_tex_type = basist::cBASISTexType2DArray;
		}

		params.m_mip_gen = false; // Mips are provided by Unreal.

		basisu::job_pool jpool(1);
		params.m_pJob_pool = &jpool;

		basisu::basis_compressor compressor;
		if (!compressor.init(params))
		{
			UE_LOG(LogTextureFormatBasis, Error, TEXT("basis_compressor::init() failed!"));
			return false;
		}

		basisu::basis_compressor::error_code result = compressor.process();
		if (result != basisu::basis_compressor::error_code::cECSuccess)
		{
			UE_LOG(LogTextureFormatBasis, Error, TEXT("basis_compressor::process() failed with error %d!"), (int)result);
			return false;
		}

		const basisu::uint8_vec& compressed_data = compressor.get_output_basis_file();

		// Transcode to the desired hardware format
		basist::basisu_transcoder_init();
		basist::basisu_transcoder transcoder;
		if (!transcoder.start_transcoding(compressed_data.data(), compressed_data.size()))
		{
			UE_LOG(LogTextureFormatBasis, Error, TEXT("Failed to start transcoding basisu file!"));
			return false;
		}

		OutImage.PixelFormat = CompressedPixelFormat;
		OutImage.SizeX = InImage.SizeX;
		OutImage.SizeY = InImage.SizeY;
		OutImage.NumSlicesWithDepth = InImage.NumSlices;

		basist::transcoder_texture_format BasisFormat = BasisFormatFromPixelFormat(CompressedPixelFormat);
		if (BasisFormat == basist::transcoder_texture_format::cTFTotalTextureFormats)
		{
			UE_LOG(LogTextureFormatBasis, Error, TEXT("Unsupported pixel format for transcoding: %s"), GPixelFormats[CompressedPixelFormat].Name);
			return false;
		}

		if (bIsHDR)
		{
			if (CompressedPixelFormat == PF_ASTC_4x4)
			{
				BasisFormat = basist::transcoder_texture_format::cTFASTC_HDR_4x4_RGBA;
			}
		}

		basisu::gpu_image TmpGpuImage;
		basisu::texture_format BasisTextureFormat = basis_get_basisu_texture_format(BasisFormat);
		TmpGpuImage.init(BasisTextureFormat, InImage.SizeX, InImage.SizeY);

		const uint32_t BlockSize = TmpGpuImage.get_bytes_per_block();
		const uint32_t NumBlocksX = (InImage.SizeX + 3) / 4;
		const uint32_t NumBlocksY = (InImage.SizeY + 3) / 4;
		const uint32_t BytesPerSlice = NumBlocksX * NumBlocksY * BlockSize;

		OutImage.RawData.AddUninitialized(BytesPerSlice * InImage.NumSlices);

		for (int32 Slice = 0; Slice < InImage.NumSlices; ++Slice)
		{
			void* DestSliceData = OutImage.RawData.GetData() + (BytesPerSlice * Slice);
			if (!transcoder.transcode_image_level(
				compressed_data.data(), compressed_data.size(),
				Slice, 0,
				DestSliceData, NumBlocksX * NumBlocksY,
				BasisFormat))
			{
				UE_LOG(LogTextureFormatBasis, Error, TEXT("Failed to transcode slice %d!"), Slice);
				return false;
			}
		}

		return true;
	}
	
	// noinline so we see it on the call stack :
	FORCENOINLINE void CheckMemoryIsReadable(const void * Buffer,int64 Size) const
	{
		check( Size > 0 );

		const uint8 * Start = (const uint8 *)Buffer;

		// volatile to ensure it's not optimized out
		volatile uint8 Byte;

		Byte = Start[0];
		Byte = Start[Size-1];
	}

};

#endif // WITH_BASIS_COMPRESSOR
//===============================================================

// TFB_ plugins to Basis to run Basis system services in Unreal

// Init is only done once for all versions
static void TFB_Plugins_Init()
{
	// Install Unreal system plugins to Basis
	// this should only be done once
	// and should be done before any other Basis calls
}

// Install is done for each Basis DLL
static void TFB_Plugins_Install(const FBasisTextureVTable * VTable)
{
}

//=========================================================================

class FTextureFormatBasisModule : public ITextureFormatModule
{
public:

	ITextureFormat* TextureFormat = nullptr;

	FTextureFormatBasisModule() { }
	virtual ~FTextureFormatBasisModule()
	{
		if ( TextureFormat )
		{
			delete TextureFormat;
			TextureFormat = nullptr;
		}
	}

	virtual void StartupModule() override
	{
	}

	virtual bool CanCallGetTextureFormats() override { return false; }

	virtual ITextureFormat* GetTextureFormat()
	{
#if WITH_BASIS_COMPRESSOR
		// this is called several times during normal init
		
		auto MakeTextureFormat = [&]()
		{
			check( TextureFormat == nullptr );
			FTextureFormatBasis * ptr = new FTextureFormatBasis();
			if ( ptr->Init() )
			{
				TextureFormat = ptr;
			}
			else
			{
				UE_LOG(LogTextureFormatBasis,Warning,TEXT("Basis Texture Init failed, not installed"));
				delete ptr;
		}
		};
		
		UE_CALL_ONCE( MakeTextureFormat );

		return TextureFormat;
#else
		return nullptr;
#endif // WITH_BASIS_COMPRESSOR
	}

#if WITH_BASIS_COMPRESSOR
	static inline UE::DerivedData::TBuildFunctionFactory<FBasisTextureBuildFunction> BuildFunctionFactory;
#endif // WITH_BASIS_COMPRESSOR
};

IMPLEMENT_MODULE(FTextureFormatBasisModule, TextureFormatBasis);

