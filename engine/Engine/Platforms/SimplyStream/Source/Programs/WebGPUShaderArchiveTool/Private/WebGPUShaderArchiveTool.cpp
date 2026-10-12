// Copyright Epic Games, Inc. All Rights Reserved.
//
// Offline helper for the WebGPU shader archive size work.
//
// ExtractDecompressed reads a loose UE shader archive, uses RenderCore's existing
// Oodle decompression path, and writes one uncompressed serialized shader blob per
// shader. Feed the output directory to Tools/shader-archive/analyze-webgpu-shader-archive.mjs
// via --blob-dir for exact WebGPU header/resource/pWGSL/optional-data breakdowns.

#include <CoreGlobals.h>
#include <Compression/OodleDataCompression.h>
#include <HAL/LowLevelMemTracker.h>
#include <LaunchEngineLoop.h>
#include <Modules/ModuleManager.h>
#include <Misc/ScopeExit.h>
#include <RequiredProgramMainCPPInclude.h>

#include "Containers/ArrayView.h"
#include "HAL/FileManager.h"
#include "Hash/ShaderHash.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "Misc/Crc.h"
#include "Misc/SecureHash.h"
#include "Serialization/MemoryWriter.h"
#include "ShaderCodeArchive.h"

IMPLEMENT_APPLICATION(WebGPUShaderArchiveTool, "WebGPUShaderArchiveTool");

DEFINE_LOG_CATEGORY_STATIC(LogWebGPUShaderArchiveTool, Log, All);

namespace UE::SimplyStream::ShaderArchiveTool
{
static constexpr uint32 WebGPUShaderArchiveRedirectMagic = 0x52534757u; // 'WGSR'
static constexpr uint32 WebGPUShaderArchiveRedirectVersion = 1u;
static constexpr uint32 WebGPUShaderArchiveRedirectFlagCompressedGroup = 1u << 0;

static FString JsonEscape(const FString& In)
{
	FString Out;
	Out.Reserve(In.Len() + 8);
	for (const TCHAR Ch : In)
	{
		switch (Ch)
		{
		case TEXT('\\'): Out += TEXT("\\\\"); break;
		case TEXT('"'): Out += TEXT("\\\""); break;
		case TEXT('\n'): Out += TEXT("\\n"); break;
		case TEXT('\r'): Out += TEXT("\\r"); break;
		case TEXT('\t'): Out += TEXT("\\t"); break;
		default:
			if (Ch < 0x20)
			{
				Out += FString::Printf(TEXT("\\u%04x"), uint32(Ch));
			}
			else
			{
				Out.AppendChar(Ch);
			}
			break;
		}
	}
	return Out;
}

static void AppendJsonString(FString& Out, const FString& Value)
{
	Out += TEXT("\"");
	Out += JsonEscape(Value);
	Out += TEXT("\"");
}

static FString Sha1String(TConstArrayView<uint8> Bytes)
{
	uint8 Digest[FSHA1::DigestSize];
	static const uint8 EmptyByte = 0;
	const uint8* Data = Bytes.Num() > 0 ? Bytes.GetData() : &EmptyByte;
	FSHA1::HashBuffer(Data, Bytes.Num(), Digest);

	FString Out;
	Out.Reserve(FSHA1::DigestSize * 2);
	for (uint8 Byte : Digest)
	{
		Out += FString::Printf(TEXT("%02x"), Byte);
	}
	return Out;
}

static bool LoadArchive(const FString& ArchivePath, FSerializedShaderArchive& OutArchive, int64& OutCodeStartOffset)
{
	TUniquePtr<FArchive> Reader(IFileManager::Get().CreateFileReader(*ArchivePath));
	if (!Reader)
	{
		UE_LOG(LogWebGPUShaderArchiveTool, Error, TEXT("Failed to open archive '%s'"), *ArchivePath);
		return false;
	}

	if (!FSerializedShaderArchive::SerializeHeaderVersion(*Reader))
	{
		UE_LOG(LogWebGPUShaderArchiveTool, Error, TEXT("Archive '%s' has an unsupported shader archive version"), *ArchivePath);
		return false;
	}

	*Reader << OutArchive;
	if (Reader->IsError())
	{
		UE_LOG(LogWebGPUShaderArchiveTool, Error, TEXT("Failed to deserialize archive header '%s'"), *ArchivePath);
		return false;
	}

	OutCodeStartOffset = Reader->Tell();
	return true;
}

static bool ReadCompressedPayload(const FString& ArchivePath, const FShaderCodeEntry& Entry, int64 CodeStartOffset, TArray<uint8>& OutPayload)
{
	TUniquePtr<FArchive> Reader(IFileManager::Get().CreateFileReader(*ArchivePath));
	if (!Reader)
	{
		UE_LOG(LogWebGPUShaderArchiveTool, Error, TEXT("Failed to reopen archive '%s'"), *ArchivePath);
		return false;
	}

	const int64 FileSize = Reader->TotalSize();
	const int64 PayloadOffset = CodeStartOffset + int64(Entry.Offset);
	const int64 PayloadEnd = PayloadOffset + int64(Entry.Size);
	if (PayloadOffset < CodeStartOffset || PayloadEnd < PayloadOffset || PayloadEnd > FileSize)
	{
		UE_LOG(LogWebGPUShaderArchiveTool, Error,
			TEXT("Shader payload range is outside archive: offset=%lld size=%u codeStart=%lld fileSize=%lld"),
			PayloadOffset, Entry.Size, CodeStartOffset, FileSize);
		return false;
	}

	OutPayload.SetNumUninitialized(Entry.Size);
	Reader->Seek(PayloadOffset);
	Reader->Serialize(OutPayload.GetData(), OutPayload.Num());
	if (Reader->IsError())
	{
		UE_LOG(LogWebGPUShaderArchiveTool, Error, TEXT("Failed to read shader payload at offset %lld"), PayloadOffset);
		return false;
	}
	return true;
}

static bool WriteBytes(const FString& Path, TConstArrayView<uint8> Bytes)
{
	TUniquePtr<FArchive> Writer(IFileManager::Get().CreateFileWriter(*Path));
	if (!Writer)
	{
		UE_LOG(LogWebGPUShaderArchiveTool, Error, TEXT("Failed to open '%s' for writing"), *Path);
		return false;
	}
	Writer->Serialize(const_cast<uint8*>(Bytes.GetData()), Bytes.Num());
	return Writer->Close();
}

static bool WriteArchiveBytes(const FString& Path, TArray<uint8>& Bytes)
{
	TUniquePtr<FArchive> Writer(IFileManager::Get().CreateFileWriter(*Path));
	if (!Writer)
	{
		UE_LOG(LogWebGPUShaderArchiveTool, Error, TEXT("Failed to open '%s' for writing"), *Path);
		return false;
	}
	Writer->Serialize(Bytes.GetData(), Bytes.Num());
	return Writer->Close();
}

static FString GetArchiveFormatName(const FString& ArchivePath)
{
	FShaderLibraryNameInfo NameInfo;
	if (NameInfo.ParseFromFilename(*FPaths::GetCleanFilename(ArchivePath)))
	{
		return NameInfo.GetFormatAndPlatformName();
	}
	return TEXT("SF_WEBGPU");
}

static bool ParseByteSize(const FString& Text, uint64& OutBytes)
{
	FString Value = Text;
	Value.TrimStartAndEndInline();
	if (Value.IsEmpty())
	{
		return false;
	}

	uint64 Multiplier = 1;
	if (Value.EndsWith(TEXT("MiB"), ESearchCase::IgnoreCase))
	{
		Multiplier = 1024ull * 1024ull;
		Value.LeftChopInline(3);
	}
	else if (Value.EndsWith(TEXT("MB"), ESearchCase::IgnoreCase))
	{
		Multiplier = 1000ull * 1000ull;
		Value.LeftChopInline(2);
	}
	else if (Value.EndsWith(TEXT("KiB"), ESearchCase::IgnoreCase))
	{
		Multiplier = 1024ull;
		Value.LeftChopInline(3);
	}
	else if (Value.EndsWith(TEXT("KB"), ESearchCase::IgnoreCase))
	{
		Multiplier = 1000ull;
		Value.LeftChopInline(2);
	}
	else if (Value.EndsWith(TEXT("B"), ESearchCase::IgnoreCase))
	{
		Value.LeftChopInline(1);
	}

	Value.TrimStartAndEndInline();
	if (Value.IsEmpty())
	{
		return false;
	}

	const uint64 Parsed = FCString::Strtoui64(*Value, nullptr, 10);
	if (Parsed == 0 && Value != TEXT("0"))
	{
		return false;
	}
	OutBytes = Parsed * Multiplier;
	return true;
}

static bool ParseOodleCompressor(const FString& Value, FOodleDataCompression::ECompressor& OutCompressor)
{
	if (Value.IsEmpty())
	{
		OutCompressor = FOodleDataCompression::ECompressor::Mermaid;
		return true;
	}
	return FOodleDataCompression::ECompressorFromString(Value, OutCompressor);
}

static bool ParseOodleLevel(const FString& Value, FOodleDataCompression::ECompressionLevel& OutLevel)
{
	if (Value.IsEmpty())
	{
		OutLevel = FOodleDataCompression::ECompressionLevel::Normal;
		return true;
	}
	return FOodleDataCompression::ECompressionLevelFromString(*Value, OutLevel);
}

struct FWebGPUShaderBlobSections
{
	bool bParsed = false;
	uint32 MetadataPrefixOffset = 0;
	uint32 MetadataPrefixSize = 0;
	uint32 WgslSizeFieldOffset = 0;
	uint32 WgslSizeFieldSize = 0;
	uint32 WgslPayloadOffset = 0;
	uint32 WgslPayloadSize = 0;
	uint32 OptionalDataOffset = 0;
	uint32 OptionalDataSize = 0;
	uint32 ActualShaderSize = 0;
	FString MetadataPrefixSha1;
	FString WgslPayloadSha1;
	FString OptionalDataSha1;
};

struct FLoadedShaderBlob
{
	int32 Index = INDEX_NONE;
	FString Hash;
	EShaderFrequency Frequency = SF_NumFrequencies;
	uint64 LegacyOffset = 0;
	uint32 StoredSize = 0;
	uint32 UncompressedSize = 0;
	bool bFoundPackedWgsl = false;
	uint32 WgslPayloadOffset = 0;
	uint32 WgslPayloadSize = 0;
	uint32 WgslUnpackedSize = 0;
	uint32 WgslLengthBucket = 0;
	uint32 WgslTokenPrefixCrc = 0;
	FWebGPUShaderBlobSections Sections;
	TArray<uint8> Bytes;
};

struct FGroupedShaderPlacement
{
	int32 GroupIndex = INDEX_NONE;
	uint32 OffsetInGroup = 0;
};

struct FGroupedShaderPrototypeGroup
{
	TArray<int32> ShaderIndices;
	uint32 UncompressedSize = 0;
	uint32 StoredSize = 0;
	bool bCompressed = false;
	FString File;
};

struct FCompressedPrototypePayload
{
	uint32 UncompressedSize = 0;
	uint32 StoredSize = 0;
	bool bCompressed = false;
	TArray<uint8> StoredBytes;
};

struct FParsedRedirectShaderBlob
{
	uint32 Flags = 0;
	uint32 OriginalSerializedSize = 0;
	uint32 GroupIndex = 0;
	uint32 OffsetInGroup = 0;
	uint32 SizeInGroup = 0;
	uint32 GroupUncompressedSize = 0;
	uint32 GroupStoredSize = 0;
	FString RelativeGroupPath;
};

struct FWgslSimilarityInfo
{
	bool bFoundPackedWgsl = false;
	uint32 PayloadOffset = 0;
	uint32 PayloadSize = 0;
	uint32 UnpackedSize = 0;
	uint32 LengthBucket = 0;
	uint32 TokenPrefixCrc = 0;
};

static bool ReadUInt32LE(TConstArrayView<uint8> Bytes, int32 Offset, uint32& OutValue)
{
	if (Offset < 0 || Offset + int32(sizeof(uint32)) > Bytes.Num())
	{
		return false;
	}
	FMemory::Memcpy(&OutValue, Bytes.GetData() + Offset, sizeof(uint32));
	return true;
}

static bool ReadUInt32LEAndAdvance(TConstArrayView<uint8> Bytes, int32& Offset, uint32& OutValue)
{
	if (!ReadUInt32LE(Bytes, Offset, OutValue))
	{
		return false;
	}
	Offset += sizeof(uint32);
	return true;
}

static FWgslSimilarityInfo BuildWgslSimilarityInfo(TConstArrayView<uint8> ShaderBytes)
{
	static constexpr uint32 WgslPackMagic = 0x4C534757u; // 'WGSL'
	static constexpr uint32 WgslPackVersion = 2;
	static constexpr uint32 WgslPackHeaderSize = 20;
	static constexpr uint32 WgslPackFlagHasNames = 0x01;

	FWgslSimilarityInfo Info;

	for (int32 Offset = int32(sizeof(uint32)); Offset + int32(WgslPackHeaderSize) <= ShaderBytes.Num(); ++Offset)
	{
		uint32 Magic = 0;
		uint32 Version = 0;
		uint32 PayloadSize = 0;
		uint32 UnpackedSize = 0;
		uint32 Flags = 0;
		uint32 NameCount = 0;
		if (!ReadUInt32LE(ShaderBytes, Offset, Magic) ||
			Magic != WgslPackMagic ||
			!ReadUInt32LE(ShaderBytes, Offset + 4, Version) ||
			Version != WgslPackVersion ||
			!ReadUInt32LE(ShaderBytes, Offset - int32(sizeof(uint32)), PayloadSize) ||
			PayloadSize < WgslPackHeaderSize ||
			Offset + int32(PayloadSize) > ShaderBytes.Num() ||
			!ReadUInt32LE(ShaderBytes, Offset + 8, UnpackedSize) ||
			UnpackedSize == 0 ||
			UnpackedSize > 256u * 1024u * 1024u ||
			!ReadUInt32LE(ShaderBytes, Offset + 12, Flags) ||
			!ReadUInt32LE(ShaderBytes, Offset + 16, NameCount))
		{
			continue;
		}

		int32 TokenOffset = Offset + int32(WgslPackHeaderSize);
		if ((Flags & WgslPackFlagHasNames) != 0 || NameCount > 0)
		{
			bool bNamesValid = true;
			for (uint32 NameIndex = 0; NameIndex < NameCount; ++NameIndex)
			{
				if (TokenOffset + 4 > Offset + int32(PayloadSize))
				{
					bNamesValid = false;
					break;
				}

				uint16 NameLength = 0;
				FMemory::Memcpy(&NameLength, ShaderBytes.GetData() + TokenOffset + 2, sizeof(uint16));
				TokenOffset += 4 + int32(NameLength);
				if (TokenOffset > Offset + int32(PayloadSize))
				{
					bNamesValid = false;
					break;
				}
			}

			if (!bNamesValid)
			{
				TokenOffset = Offset + int32(WgslPackHeaderSize);
			}
		}

		const int32 TokenBytes = FMath::Max(0, Offset + int32(PayloadSize) - TokenOffset);
		const int32 HashBytes = FMath::Min(TokenBytes, 4096);
		Info.bFoundPackedWgsl = true;
		Info.PayloadOffset = uint32(Offset);
		Info.PayloadSize = PayloadSize;
		Info.UnpackedSize = UnpackedSize;
		Info.LengthBucket = UnpackedSize / 4096u;
		Info.TokenPrefixCrc = HashBytes > 0
			? FCrc::MemCrc32(ShaderBytes.GetData() + TokenOffset, HashBytes)
			: 0;
		return Info;
	}

	const int32 FallbackHashBytes = FMath::Min(ShaderBytes.Num(), 4096);
	Info.PayloadOffset = 0;
	Info.PayloadSize = ShaderBytes.Num();
	Info.UnpackedSize = ShaderBytes.Num();
	Info.LengthBucket = uint32(ShaderBytes.Num()) / 4096u;
	Info.TokenPrefixCrc = FallbackHashBytes > 0 ? FCrc::MemCrc32(ShaderBytes.GetData(), FallbackHashBytes) : 0;
	return Info;
}

static TConstArrayView<uint8> SliceBytes(TConstArrayView<uint8> Bytes, uint32 Offset, uint32 Size)
{
	if (Offset > uint32(Bytes.Num()) || Size > uint32(Bytes.Num()) - Offset)
	{
		return TConstArrayView<uint8>();
	}
	return TConstArrayView<uint8>(Bytes.GetData() + Offset, Size);
}

static void AppendUInt16LE(TArray<uint8>& OutBytes, uint16 Value)
{
	const int32 Offset = OutBytes.AddUninitialized(sizeof(uint16));
	FMemory::Memcpy(OutBytes.GetData() + Offset, &Value, sizeof(uint16));
}

static void AppendUInt32LE(TArray<uint8>& OutBytes, uint32 Value)
{
	const int32 Offset = OutBytes.AddUninitialized(sizeof(uint32));
	FMemory::Memcpy(OutBytes.GetData() + Offset, &Value, sizeof(uint32));
}

static void AppendUtf8StringBytes(TArray<uint8>& OutBytes, const FString& Value)
{
	FTCHARToUTF8 Utf8(*Value);
	OutBytes.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
}

static FWebGPUShaderBlobSections ParseWebGPUShaderBlobSections(TConstArrayView<uint8> ShaderBytes)
{
	FWebGPUShaderBlobSections Sections;
	const FWgslSimilarityInfo WgslInfo = BuildWgslSimilarityInfo(ShaderBytes);
	if (!WgslInfo.bFoundPackedWgsl || WgslInfo.PayloadOffset < sizeof(uint32))
	{
		return Sections;
	}

	const uint32 WgslPayloadEnd = WgslInfo.PayloadOffset + WgslInfo.PayloadSize;
	if (WgslPayloadEnd < WgslInfo.PayloadOffset || WgslPayloadEnd > uint32(ShaderBytes.Num()))
	{
		return Sections;
	}

	const uint32 WgslSizeFieldOffset = WgslInfo.PayloadOffset - sizeof(uint32);
	Sections.bParsed = true;
	Sections.MetadataPrefixOffset = 0;
	Sections.MetadataPrefixSize = WgslSizeFieldOffset;
	Sections.WgslSizeFieldOffset = WgslSizeFieldOffset;
	Sections.WgslSizeFieldSize = sizeof(uint32);
	Sections.WgslPayloadOffset = WgslInfo.PayloadOffset;
	Sections.WgslPayloadSize = WgslInfo.PayloadSize;
	Sections.OptionalDataOffset = WgslPayloadEnd;
	Sections.OptionalDataSize = uint32(ShaderBytes.Num() - WgslPayloadEnd);
	Sections.ActualShaderSize = WgslPayloadEnd;
	Sections.MetadataPrefixSha1 = Sha1String(SliceBytes(ShaderBytes, Sections.MetadataPrefixOffset, Sections.MetadataPrefixSize));
	Sections.WgslPayloadSha1 = Sha1String(SliceBytes(ShaderBytes, Sections.WgslPayloadOffset, Sections.WgslPayloadSize));
	Sections.OptionalDataSha1 = Sha1String(SliceBytes(ShaderBytes, Sections.OptionalDataOffset, Sections.OptionalDataSize));
	return Sections;
}

static bool LoadAllShaderBlobs(const FString& ArchivePath, const FSerializedShaderArchive& SerializedShaders, int64 CodeStartOffset, TArray<FLoadedShaderBlob>& OutShaders)
{
	const TArrayView<const FShaderCodeEntry> ShaderEntries = SerializedShaders.GetShaderEntries();
	const TArrayView<const FShaderHash> ShaderHashes = SerializedShaders.GetShaderHashes();
	OutShaders.Empty(ShaderEntries.Num());

	TArray<uint8> CompressedPayload;
	for (int32 ShaderIndex = 0; ShaderIndex < ShaderEntries.Num(); ++ShaderIndex)
	{
		const FShaderCodeEntry& Entry = ShaderEntries[ShaderIndex];
		if (!ReadCompressedPayload(ArchivePath, Entry, CodeStartOffset, CompressedPayload))
		{
			return false;
		}

		FLoadedShaderBlob& Shader = OutShaders.AddDefaulted_GetRef();
		Shader.Index = ShaderIndex;
		Shader.Hash = ShaderHashes.IsValidIndex(ShaderIndex)
			? ShaderHashes[ShaderIndex].ToString()
			: FString::Printf(TEXT("missinghash%06d"), ShaderIndex);
		Shader.Frequency = Entry.GetFrequency();
		Shader.LegacyOffset = Entry.Offset;
		Shader.StoredSize = Entry.Size;
		Shader.UncompressedSize = Entry.GetUncompressedSize();
		FSerializedShaderArchive::DecompressShaderEntryAndAppend(Entry, CompressedPayload.GetData(), Shader.Bytes);

		if (Shader.Bytes.Num() != int32(Shader.UncompressedSize))
		{
			UE_LOG(LogWebGPUShaderArchiveTool, Error,
				TEXT("Shader %d decompressed to %d bytes, expected %u"),
				ShaderIndex,
				Shader.Bytes.Num(),
				Shader.UncompressedSize);
			return false;
		}

		const FWgslSimilarityInfo SimilarityInfo = BuildWgslSimilarityInfo(Shader.Bytes);
		Shader.bFoundPackedWgsl = SimilarityInfo.bFoundPackedWgsl;
		Shader.WgslPayloadOffset = SimilarityInfo.PayloadOffset;
		Shader.WgslPayloadSize = SimilarityInfo.PayloadSize;
		Shader.WgslUnpackedSize = SimilarityInfo.UnpackedSize;
		Shader.WgslLengthBucket = SimilarityInfo.LengthBucket;
		Shader.WgslTokenPrefixCrc = SimilarityInfo.TokenPrefixCrc;
		Shader.Sections = ParseWebGPUShaderBlobSections(Shader.Bytes);
	}

	return true;
}

static void AddSizeBoundedGroups(const TArray<int32>& OrderedShaderIndices, const TArray<FLoadedShaderBlob>& Shaders, uint64 MaxGroupSize, TArray<FGroupedShaderPrototypeGroup>& OutGroups)
{
	check(MaxGroupSize > 0);

	FGroupedShaderPrototypeGroup CurrentGroup;
	for (int32 ShaderIndex : OrderedShaderIndices)
	{
		const uint32 ShaderSize = Shaders[ShaderIndex].UncompressedSize;
		if (!CurrentGroup.ShaderIndices.IsEmpty() &&
			uint64(CurrentGroup.UncompressedSize) + uint64(ShaderSize) > MaxGroupSize)
		{
			OutGroups.Add(MoveTemp(CurrentGroup));
			CurrentGroup = FGroupedShaderPrototypeGroup();
		}

		CurrentGroup.ShaderIndices.Add(ShaderIndex);
		CurrentGroup.UncompressedSize += ShaderSize;
	}

	if (!CurrentGroup.ShaderIndices.IsEmpty())
	{
		OutGroups.Add(MoveTemp(CurrentGroup));
	}
}

static bool BuildPrototypeGroupsFromPolicy(
	const FString& Policy,
	uint64 MaxGroupSize,
	const FName& Format,
	const FSerializedShaderArchive& SerializedShaders,
	const TArray<FLoadedShaderBlob>& Shaders,
	TArray<FGroupedShaderPrototypeGroup>& OutGroups,
	uint32& OutHeaderBytes)
{
	OutGroups.Empty();
	OutHeaderBytes = 0;

	if (Policy.Equals(TEXT("IoStore"), ESearchCase::IgnoreCase))
	{
		FIoStoreShaderCodeArchiveHeader Header;
		FIoStoreShaderCodeArchive::CreateIoStoreShaderCodeArchiveHeader(Format, SerializedShaders, Header);

		OutGroups.Reserve(Header.ShaderGroupEntries.Num());
		for (int32 GroupIndex = 0; GroupIndex < Header.ShaderGroupEntries.Num(); ++GroupIndex)
		{
			const FIoStoreShaderGroupEntry& HeaderGroup = Header.ShaderGroupEntries[GroupIndex];
			FGroupedShaderPrototypeGroup& Group = OutGroups.AddDefaulted_GetRef();
			Group.UncompressedSize = HeaderGroup.UncompressedSize;
			Group.ShaderIndices.Reserve(HeaderGroup.NumShaders);
			for (uint32 Offset = 0; Offset < HeaderGroup.NumShaders; ++Offset)
			{
				Group.ShaderIndices.Add(Header.ShaderIndices[HeaderGroup.ShaderIndicesOffset + Offset]);
			}
		}

		TArray<uint8> HeaderBytes;
		FMemoryWriter HeaderWriter(HeaderBytes);
		FIoStoreShaderCodeArchive::SaveIoStoreShaderCodeArchive(Header, HeaderWriter);
		OutHeaderBytes = HeaderBytes.Num();
		return true;
	}

	TArray<int32> OrderedShaderIndices;
	OrderedShaderIndices.Reserve(Shaders.Num());
	for (const FLoadedShaderBlob& Shader : Shaders)
	{
		OrderedShaderIndices.Add(Shader.Index);
	}

	if (Policy.Equals(TEXT("Solid"), ESearchCase::IgnoreCase))
	{
		MaxGroupSize = MAX_uint64;
	}
	else if (Policy.Equals(TEXT("Frequency"), ESearchCase::IgnoreCase))
	{
		OrderedShaderIndices.Sort(
			[&Shaders](int32 A, int32 B)
			{
				const FLoadedShaderBlob& ShaderA = Shaders[A];
				const FLoadedShaderBlob& ShaderB = Shaders[B];
				if (ShaderA.Frequency != ShaderB.Frequency)
				{
					return ShaderA.Frequency < ShaderB.Frequency;
				}
				if (ShaderA.UncompressedSize != ShaderB.UncompressedSize)
				{
					return ShaderA.UncompressedSize < ShaderB.UncompressedSize;
				}
				return ShaderA.Hash < ShaderB.Hash;
			});
	}
	else if (Policy.Equals(TEXT("Similarity"), ESearchCase::IgnoreCase))
	{
		OrderedShaderIndices.Sort(
			[&Shaders](int32 A, int32 B)
			{
				const FLoadedShaderBlob& ShaderA = Shaders[A];
				const FLoadedShaderBlob& ShaderB = Shaders[B];
				if (ShaderA.Frequency != ShaderB.Frequency)
				{
					return ShaderA.Frequency < ShaderB.Frequency;
				}
				if (ShaderA.WgslLengthBucket != ShaderB.WgslLengthBucket)
				{
					return ShaderA.WgslLengthBucket < ShaderB.WgslLengthBucket;
				}
				if (ShaderA.WgslTokenPrefixCrc != ShaderB.WgslTokenPrefixCrc)
				{
					return ShaderA.WgslTokenPrefixCrc < ShaderB.WgslTokenPrefixCrc;
				}
				if (ShaderA.WgslUnpackedSize != ShaderB.WgslUnpackedSize)
				{
					return ShaderA.WgslUnpackedSize < ShaderB.WgslUnpackedSize;
				}
				if (ShaderA.UncompressedSize != ShaderB.UncompressedSize)
				{
					return ShaderA.UncompressedSize < ShaderB.UncompressedSize;
				}
				return ShaderA.Hash < ShaderB.Hash;
			});
	}
	else if (!Policy.Equals(TEXT("Input"), ESearchCase::IgnoreCase))
	{
		UE_LOG(LogWebGPUShaderArchiveTool, Error, TEXT("Unknown prototype -Policy='%s'. Expected IoStore, Input, Frequency, Similarity, or Solid."), *Policy);
		return false;
	}

	AddSizeBoundedGroups(OrderedShaderIndices, Shaders, FMath::Max<uint64>(MaxGroupSize, 1), OutGroups);
	return true;
}

static bool CompressPrototypePayload(
	TConstArrayView<uint8> UncompressedBytes,
	FOodleDataCompression::ECompressor OodleCompressor,
	FOodleDataCompression::ECompressionLevel OodleLevel,
	bool bKeepStoredBytes,
	FCompressedPrototypePayload& OutPayload)
{
	OutPayload = FCompressedPrototypePayload();
	OutPayload.UncompressedSize = uint32(UncompressedBytes.Num());
	if (UncompressedBytes.IsEmpty())
	{
		return true;
	}

	int64 CompressedSizeBound = 0;
	ShaderCodeArchive::CompressShaderWithOodle(
		nullptr,
		CompressedSizeBound,
		UncompressedBytes.GetData(),
		UncompressedBytes.Num(),
		OodleCompressor,
		OodleLevel);

	TArray<uint8> CompressedBytes;
	CompressedBytes.SetNumUninitialized(CompressedSizeBound);

	int64 CompressedSize = CompressedSizeBound;
	const bool bCompressedOk = ShaderCodeArchive::CompressShaderWithOodle(
		CompressedBytes.GetData(),
		CompressedSize,
		UncompressedBytes.GetData(),
		UncompressedBytes.Num(),
		OodleCompressor,
		OodleLevel);

	OutPayload.bCompressed = bCompressedOk && CompressedSize > 0 && CompressedSize < UncompressedBytes.Num();
	OutPayload.StoredSize = OutPayload.bCompressed
		? uint32(FMath::Min<int64>(CompressedSize, MAX_uint32))
		: uint32(UncompressedBytes.Num());

	if (bKeepStoredBytes)
	{
		if (OutPayload.bCompressed)
		{
			OutPayload.StoredBytes.Append(CompressedBytes.GetData(), int32(CompressedSize));
		}
		else
		{
			OutPayload.StoredBytes.Append(UncompressedBytes.GetData(), UncompressedBytes.Num());
		}
	}

	return true;
}

static bool CompressAndWritePrototypeGroups(
	const FString& GroupDir,
	const TArray<FLoadedShaderBlob>& Shaders,
	FOodleDataCompression::ECompressor OodleCompressor,
	FOodleDataCompression::ECompressionLevel OodleLevel,
	bool bWritePayloads,
	TArray<FGroupedShaderPrototypeGroup>& Groups,
	TArray<FGroupedShaderPlacement>& OutPlacements,
	uint64& OutStoredBytes,
	uint64& OutUncompressedBytes)
{
	IFileManager::Get().MakeDirectory(*GroupDir, /*Tree*/ true);

	OutPlacements.SetNum(Shaders.Num());
	OutStoredBytes = 0;
	OutUncompressedBytes = 0;

	TArray<uint8> GroupBytes;
	for (int32 GroupIndex = 0; GroupIndex < Groups.Num(); ++GroupIndex)
	{
		FGroupedShaderPrototypeGroup& Group = Groups[GroupIndex];
		GroupBytes.Reset();
		GroupBytes.Reserve(Group.UncompressedSize);

		uint32 OffsetInGroup = 0;
		for (int32 ShaderIndex : Group.ShaderIndices)
		{
			const FLoadedShaderBlob& Shader = Shaders[ShaderIndex];
			FGroupedShaderPlacement& Placement = OutPlacements[ShaderIndex];
			Placement.GroupIndex = GroupIndex;
			Placement.OffsetInGroup = OffsetInGroup;

			GroupBytes.Append(Shader.Bytes);
			OffsetInGroup += Shader.UncompressedSize;
		}

		Group.UncompressedSize = GroupBytes.Num();
		OutUncompressedBytes += Group.UncompressedSize;

		FCompressedPrototypePayload StoredPayload;
		if (!CompressPrototypePayload(GroupBytes, OodleCompressor, OodleLevel, bWritePayloads, StoredPayload))
		{
			return false;
		}

		Group.bCompressed = StoredPayload.bCompressed;
		Group.StoredSize = StoredPayload.StoredSize;
		Group.File = FString::Printf(TEXT("groups/group-%06d.%s"), GroupIndex, Group.bCompressed ? TEXT("oodle") : TEXT("raw"));
		OutStoredBytes += Group.StoredSize;

		if (bWritePayloads)
		{
			const FString GroupPath = GroupDir / FPaths::GetCleanFilename(Group.File);
			if (!WriteBytes(GroupPath, StoredPayload.StoredBytes))
			{
				return false;
			}
		}
	}

	return true;
}

struct FSectionUniqueStats
{
	uint64 TotalBytes = 0;
	TMap<FString, uint32> UniqueBytesByHash;

	void Add(const FString& Hash, uint32 Size)
	{
		TotalBytes += Size;
		if (Size > 0 && !Hash.IsEmpty() && !UniqueBytesByHash.Contains(Hash))
		{
			UniqueBytesByHash.Add(Hash, Size);
		}
	}

	uint64 GetUniqueBytes() const
	{
		uint64 UniqueBytes = 0;
		for (const TPair<FString, uint32>& Pair : UniqueBytesByHash)
		{
			UniqueBytes += Pair.Value;
		}
		return UniqueBytes;
	}
};

struct FWebGPUShaderBlobSectionStats
{
	int32 ParsedCount = 0;
	int32 ParseFailureCount = 0;
	FSectionUniqueStats MetadataPrefix;
	FSectionUniqueStats WgslPayload;
	FSectionUniqueStats OptionalData;

	uint64 GetCurrentMetadataBytes() const
	{
		return MetadataPrefix.TotalBytes + OptionalData.TotalBytes;
	}

	uint64 GetInternedMetadataPayloadBytes() const
	{
		return MetadataPrefix.GetUniqueBytes() + OptionalData.GetUniqueBytes();
	}
};

static FWebGPUShaderBlobSectionStats BuildSectionStats(const TArray<FLoadedShaderBlob>& Shaders)
{
	FWebGPUShaderBlobSectionStats Stats;
	for (const FLoadedShaderBlob& Shader : Shaders)
	{
		const FWebGPUShaderBlobSections& Sections = Shader.Sections;
		if (!Sections.bParsed)
		{
			++Stats.ParseFailureCount;
			continue;
		}

		++Stats.ParsedCount;
		Stats.MetadataPrefix.Add(Sections.MetadataPrefixSha1, Sections.MetadataPrefixSize);
		Stats.WgslPayload.Add(Sections.WgslPayloadSha1, Sections.WgslPayloadSize);
		Stats.OptionalData.Add(Sections.OptionalDataSha1, Sections.OptionalDataSize);
	}
	return Stats;
}

static void AppendSectionUniqueStatsJson(FString& Out, const TCHAR* Name, const FSectionUniqueStats& Stats, bool bTrailingComma)
{
	const uint64 UniqueBytes = Stats.GetUniqueBytes();
	Out += FString::Printf(
		TEXT("    \"%s\": { \"totalBytes\": %llu, \"uniqueCount\": %d, \"uniqueBytes\": %llu, \"duplicateBytes\": %lld }%s\n"),
		Name,
		static_cast<unsigned long long>(Stats.TotalBytes),
		Stats.UniqueBytesByHash.Num(),
		static_cast<unsigned long long>(UniqueBytes),
		static_cast<long long>(Stats.TotalBytes) - static_cast<long long>(UniqueBytes),
		bTrailingComma ? TEXT(",") : TEXT(""));
}

static void AppendSectionStatsJson(FString& Out, const FWebGPUShaderBlobSectionStats& Stats)
{
	const uint64 CurrentMetadataBytes = Stats.GetCurrentMetadataBytes();
	const uint64 InternedPayloadBytes = Stats.GetInternedMetadataPayloadBytes();
	const uint64 ReferenceBytesEstimate = uint64(Stats.ParsedCount) * 8ull;
	const uint64 EstimatedInternedBytes = InternedPayloadBytes + ReferenceBytesEstimate;

	Out += TEXT("  \"webgpuSectionSummary\": {\n");
	Out += FString::Printf(TEXT("    \"parsedCount\": %d,\n"), Stats.ParsedCount);
	Out += FString::Printf(TEXT("    \"parseFailureCount\": %d,\n"), Stats.ParseFailureCount);
	Out += FString::Printf(TEXT("    \"currentMetadataBytes\": %llu,\n"), static_cast<unsigned long long>(CurrentMetadataBytes));
	Out += FString::Printf(TEXT("    \"internedMetadataPayloadBytes\": %llu,\n"), static_cast<unsigned long long>(InternedPayloadBytes));
	Out += FString::Printf(TEXT("    \"referenceBytesEstimate\": %llu,\n"), static_cast<unsigned long long>(ReferenceBytesEstimate));
	Out += FString::Printf(TEXT("    \"estimatedInternedMetadataBytes\": %llu,\n"), static_cast<unsigned long long>(EstimatedInternedBytes));
	Out += FString::Printf(TEXT("    \"estimatedMetadataSavingsBytes\": %lld,\n"),
		static_cast<long long>(CurrentMetadataBytes) - static_cast<long long>(EstimatedInternedBytes));
	Out += TEXT("    \"sections\": {\n");
	AppendSectionUniqueStatsJson(Out, TEXT("metadataPrefix"), Stats.MetadataPrefix, true);
	AppendSectionUniqueStatsJson(Out, TEXT("pwgslPayload"), Stats.WgslPayload, true);
	AppendSectionUniqueStatsJson(Out, TEXT("optionalData"), Stats.OptionalData, false);
	Out += TEXT("    }\n");
	Out += TEXT("  },\n");
}

struct FMetadataTableEntry
{
	FString Hash;
	uint32 Offset = 0;
	uint32 Size = 0;
};

struct FMetadataInternedTable
{
	FString Name;
	FString File;
	TArray<FMetadataTableEntry> Entries;
	uint64 SourceBytes = 0;
	uint32 UncompressedSize = 0;
	uint32 StoredSize = 0;
	bool bCompressed = false;
};

struct FMetadataInternedShaderPlacement
{
	int32 MetadataPrefixIndex = INDEX_NONE;
	int32 OptionalDataIndex = INDEX_NONE;
	int32 PayloadGroupIndex = INDEX_NONE;
	uint32 OffsetInPayloadGroup = 0;
	uint32 PayloadSize = 0;
	bool bFallbackSerializedShader = false;
};

struct FMetadataInternedPrototype
{
	bool bValid = false;
	int32 ParsedShaderCount = 0;
	int32 FallbackShaderCount = 0;
	uint64 CurrentUncompressedPayloadBytes = 0;
	uint64 InternedUncompressedPayloadBytes = 0;
	uint64 StoredPayloadBytes = 0;
	uint64 PayloadGroupStoredBytes = 0;
	uint64 PayloadGroupUncompressedBytes = 0;
	uint64 MetadataTableStoredBytes = 0;
	uint64 MetadataTableUncompressedBytes = 0;
	uint64 ReferenceBytesEstimate = 0;
	uint64 IndexBytesEstimate = 0;
	FMetadataInternedTable MetadataPrefixTable;
	FMetadataInternedTable OptionalDataTable;
	TArray<FGroupedShaderPrototypeGroup> PayloadGroups;
	TArray<FMetadataInternedShaderPlacement> ShaderPlacements;
};

static int32 AddUniqueMetadataSlice(
	TConstArrayView<uint8> Bytes,
	const FString& Hash,
	TMap<FString, int32>& InOutIndexByHash,
	TArray<FMetadataTableEntry>& InOutEntries,
	TArray<uint8>& InOutTableBytes)
{
	if (Bytes.IsEmpty())
	{
		return INDEX_NONE;
	}

	if (const int32* ExistingIndex = InOutIndexByHash.Find(Hash))
	{
		return *ExistingIndex;
	}

	const int32 NewIndex = InOutEntries.Num();
	FMetadataTableEntry& Entry = InOutEntries.AddDefaulted_GetRef();
	Entry.Hash = Hash;
	Entry.Offset = InOutTableBytes.Num();
	Entry.Size = Bytes.Num();
	InOutTableBytes.Append(Bytes.GetData(), Bytes.Num());
	InOutIndexByHash.Add(Hash, NewIndex);
	return NewIndex;
}

static bool CompressMetadataTable(
	FMetadataInternedTable& Table,
	TConstArrayView<uint8> TableBytes,
	const FString& TableDir,
	FOodleDataCompression::ECompressor OodleCompressor,
	FOodleDataCompression::ECompressionLevel OodleLevel,
	bool bWritePayloads)
{
	FCompressedPrototypePayload StoredPayload;
	if (!CompressPrototypePayload(TableBytes, OodleCompressor, OodleLevel, bWritePayloads, StoredPayload))
	{
		return false;
	}

	Table.UncompressedSize = StoredPayload.UncompressedSize;
	Table.StoredSize = StoredPayload.StoredSize;
	Table.bCompressed = StoredPayload.bCompressed;
	if (Table.UncompressedSize > 0)
	{
		Table.File = FString::Printf(TEXT("metadata-interned/tables/%s.%s"), *Table.Name, Table.bCompressed ? TEXT("oodle") : TEXT("raw"));
		if (bWritePayloads)
		{
			const FString TablePath = TableDir / FPaths::GetCleanFilename(Table.File);
			if (!WriteBytes(TablePath, StoredPayload.StoredBytes))
			{
				return false;
			}
		}
	}
	return true;
}

static bool BuildMetadataInternedPrototype(
	const FString& OutDir,
	const TArray<FLoadedShaderBlob>& Shaders,
	const TArray<FGroupedShaderPrototypeGroup>& SourceGroups,
	FOodleDataCompression::ECompressor OodleCompressor,
	FOodleDataCompression::ECompressionLevel OodleLevel,
	bool bWritePayloads,
	FMetadataInternedPrototype& OutPrototype)
{
	OutPrototype = FMetadataInternedPrototype();
	OutPrototype.bValid = true;
	OutPrototype.MetadataPrefixTable.Name = TEXT("metadata-prefix-table");
	OutPrototype.OptionalDataTable.Name = TEXT("optional-data-table");
	OutPrototype.ShaderPlacements.SetNum(Shaders.Num());

	const FString InternedDir = OutDir / TEXT("metadata-interned");
	const FString TableDir = InternedDir / TEXT("tables");
	const FString GroupDir = InternedDir / TEXT("groups");
	IFileManager::Get().MakeDirectory(*TableDir, /*Tree*/ true);
	IFileManager::Get().MakeDirectory(*GroupDir, /*Tree*/ true);

	TMap<FString, int32> MetadataPrefixIndexByHash;
	TMap<FString, int32> OptionalDataIndexByHash;
	TArray<uint8> MetadataPrefixTableBytes;
	TArray<uint8> OptionalDataTableBytes;

	for (const FLoadedShaderBlob& Shader : Shaders)
	{
		OutPrototype.CurrentUncompressedPayloadBytes += Shader.UncompressedSize;

		const FWebGPUShaderBlobSections& Sections = Shader.Sections;
		if (!Sections.bParsed)
		{
			++OutPrototype.FallbackShaderCount;
			continue;
		}

		++OutPrototype.ParsedShaderCount;
		FMetadataInternedShaderPlacement& Placement = OutPrototype.ShaderPlacements[Shader.Index];
		Placement.MetadataPrefixIndex = AddUniqueMetadataSlice(
			SliceBytes(Shader.Bytes, Sections.MetadataPrefixOffset, Sections.MetadataPrefixSize),
			Sections.MetadataPrefixSha1,
			MetadataPrefixIndexByHash,
			OutPrototype.MetadataPrefixTable.Entries,
			MetadataPrefixTableBytes);
		Placement.OptionalDataIndex = AddUniqueMetadataSlice(
			SliceBytes(Shader.Bytes, Sections.OptionalDataOffset, Sections.OptionalDataSize),
			Sections.OptionalDataSha1,
			OptionalDataIndexByHash,
			OutPrototype.OptionalDataTable.Entries,
			OptionalDataTableBytes);
		OutPrototype.MetadataPrefixTable.SourceBytes += Sections.MetadataPrefixSize;
		OutPrototype.OptionalDataTable.SourceBytes += Sections.OptionalDataSize;
	}

	if (!CompressMetadataTable(OutPrototype.MetadataPrefixTable, MetadataPrefixTableBytes, TableDir, OodleCompressor, OodleLevel, bWritePayloads) ||
		!CompressMetadataTable(OutPrototype.OptionalDataTable, OptionalDataTableBytes, TableDir, OodleCompressor, OodleLevel, bWritePayloads))
	{
		return false;
	}

	OutPrototype.PayloadGroups.Reserve(SourceGroups.Num());
	TArray<uint8> PayloadGroupBytes;
	for (int32 GroupIndex = 0; GroupIndex < SourceGroups.Num(); ++GroupIndex)
	{
		const FGroupedShaderPrototypeGroup& SourceGroup = SourceGroups[GroupIndex];
		FGroupedShaderPrototypeGroup& PayloadGroup = OutPrototype.PayloadGroups.AddDefaulted_GetRef();
		PayloadGroup.ShaderIndices = SourceGroup.ShaderIndices;

		PayloadGroupBytes.Reset();
		for (int32 ShaderIndex : SourceGroup.ShaderIndices)
		{
			const FLoadedShaderBlob& Shader = Shaders[ShaderIndex];
			const FWebGPUShaderBlobSections& Sections = Shader.Sections;
			FMetadataInternedShaderPlacement& Placement = OutPrototype.ShaderPlacements[ShaderIndex];
			Placement.PayloadGroupIndex = GroupIndex;
			Placement.OffsetInPayloadGroup = PayloadGroupBytes.Num();

			if (Sections.bParsed)
			{
				const TConstArrayView<uint8> PayloadBytes = SliceBytes(Shader.Bytes, Sections.WgslPayloadOffset, Sections.WgslPayloadSize);
				PayloadGroupBytes.Append(PayloadBytes.GetData(), PayloadBytes.Num());
				Placement.PayloadSize = PayloadBytes.Num();
			}
			else
			{
				PayloadGroupBytes.Append(Shader.Bytes);
				Placement.PayloadSize = Shader.Bytes.Num();
				Placement.bFallbackSerializedShader = true;
			}
		}

		FCompressedPrototypePayload StoredPayload;
		if (!CompressPrototypePayload(PayloadGroupBytes, OodleCompressor, OodleLevel, bWritePayloads, StoredPayload))
		{
			return false;
		}

		PayloadGroup.UncompressedSize = StoredPayload.UncompressedSize;
		PayloadGroup.StoredSize = StoredPayload.StoredSize;
		PayloadGroup.bCompressed = StoredPayload.bCompressed;
		PayloadGroup.File = FString::Printf(TEXT("metadata-interned/groups/payload-group-%06d.%s"), GroupIndex, PayloadGroup.bCompressed ? TEXT("oodle") : TEXT("raw"));
		OutPrototype.PayloadGroupUncompressedBytes += PayloadGroup.UncompressedSize;
		OutPrototype.PayloadGroupStoredBytes += PayloadGroup.StoredSize;

		if (bWritePayloads)
		{
			const FString PayloadGroupPath = GroupDir / FPaths::GetCleanFilename(PayloadGroup.File);
			if (!WriteBytes(PayloadGroupPath, StoredPayload.StoredBytes))
			{
				return false;
			}
		}
	}

	OutPrototype.MetadataTableUncompressedBytes =
		uint64(OutPrototype.MetadataPrefixTable.UncompressedSize) +
		uint64(OutPrototype.OptionalDataTable.UncompressedSize);
	OutPrototype.MetadataTableStoredBytes =
		uint64(OutPrototype.MetadataPrefixTable.StoredSize) +
		uint64(OutPrototype.OptionalDataTable.StoredSize);
	OutPrototype.InternedUncompressedPayloadBytes =
		OutPrototype.MetadataTableUncompressedBytes +
		OutPrototype.PayloadGroupUncompressedBytes;
	OutPrototype.StoredPayloadBytes =
		OutPrototype.MetadataTableStoredBytes +
		OutPrototype.PayloadGroupStoredBytes;
	OutPrototype.ReferenceBytesEstimate = uint64(Shaders.Num()) * 16ull;
	OutPrototype.IndexBytesEstimate =
		uint64(OutPrototype.PayloadGroups.Num()) * 32ull +
		uint64(Shaders.Num()) * 24ull +
		uint64(OutPrototype.MetadataPrefixTable.Entries.Num() + OutPrototype.OptionalDataTable.Entries.Num()) * 24ull;
	return true;
}

static void AppendMetadataInternedTableJson(FString& Out, const TCHAR* Name, const FMetadataInternedTable& Table, bool bTrailingComma)
{
	Out += FString::Printf(TEXT("      \"%s\": {\n"), Name);
	Out += FString::Printf(TEXT("        \"sourceBytes\": %llu,\n"), static_cast<unsigned long long>(Table.SourceBytes));
	Out += FString::Printf(TEXT("        \"uniqueCount\": %d,\n"), Table.Entries.Num());
	Out += FString::Printf(TEXT("        \"uncompressedSize\": %u,\n"), Table.UncompressedSize);
	Out += FString::Printf(TEXT("        \"storedSize\": %u,\n"), Table.StoredSize);
	Out += FString::Printf(TEXT("        \"compressed\": %s,\n"), Table.bCompressed ? TEXT("true") : TEXT("false"));
	Out += TEXT("        \"file\": ");
	AppendJsonString(Out, Table.File);
	Out += TEXT("\n");
	Out += FString::Printf(TEXT("      }%s\n"), bTrailingComma ? TEXT(",") : TEXT(""));
}

static void AppendMetadataInternedPrototypeJson(
	FString& Out,
	const FMetadataInternedPrototype& Prototype,
	uint64 LegacyStoredBytes,
	uint64 LegacyHeaderBytes)
{
	const uint64 TotalEstimateBytes = LegacyHeaderBytes + Prototype.IndexBytesEstimate + Prototype.StoredPayloadBytes;

	Out += TEXT("  \"metadataInternedPrototype\": {\n");
	Out += TEXT("    \"encoding\": \"metadata-interned-pwgsl-groups-v1\",\n");
	Out += FString::Printf(TEXT("    \"valid\": %s,\n"), Prototype.bValid ? TEXT("true") : TEXT("false"));
	Out += FString::Printf(TEXT("    \"parsedShaderCount\": %d,\n"), Prototype.ParsedShaderCount);
	Out += FString::Printf(TEXT("    \"fallbackShaderCount\": %d,\n"), Prototype.FallbackShaderCount);
	Out += FString::Printf(TEXT("    \"currentUncompressedPayloadBytes\": %llu,\n"), static_cast<unsigned long long>(Prototype.CurrentUncompressedPayloadBytes));
	Out += FString::Printf(TEXT("    \"internedUncompressedPayloadBytes\": %llu,\n"), static_cast<unsigned long long>(Prototype.InternedUncompressedPayloadBytes));
	Out += FString::Printf(TEXT("    \"storedPayloadBytes\": %llu,\n"), static_cast<unsigned long long>(Prototype.StoredPayloadBytes));
	Out += FString::Printf(TEXT("    \"payloadGroupStoredBytes\": %llu,\n"), static_cast<unsigned long long>(Prototype.PayloadGroupStoredBytes));
	Out += FString::Printf(TEXT("    \"payloadGroupUncompressedBytes\": %llu,\n"), static_cast<unsigned long long>(Prototype.PayloadGroupUncompressedBytes));
	Out += FString::Printf(TEXT("    \"metadataTableStoredBytes\": %llu,\n"), static_cast<unsigned long long>(Prototype.MetadataTableStoredBytes));
	Out += FString::Printf(TEXT("    \"metadataTableUncompressedBytes\": %llu,\n"), static_cast<unsigned long long>(Prototype.MetadataTableUncompressedBytes));
	Out += FString::Printf(TEXT("    \"payloadSavingsBytes\": %lld,\n"), static_cast<long long>(LegacyStoredBytes) - static_cast<long long>(Prototype.StoredPayloadBytes));
	Out += FString::Printf(TEXT("    \"payloadRatio\": %.6f,\n"), LegacyStoredBytes > 0 ? double(Prototype.StoredPayloadBytes) / double(LegacyStoredBytes) : 0.0);
	Out += FString::Printf(TEXT("    \"referenceBytesEstimate\": %llu,\n"), static_cast<unsigned long long>(Prototype.ReferenceBytesEstimate));
	Out += FString::Printf(TEXT("    \"indexBytesEstimate\": %llu,\n"), static_cast<unsigned long long>(Prototype.IndexBytesEstimate));
	Out += FString::Printf(TEXT("    \"totalEstimateBytes\": %llu,\n"), static_cast<unsigned long long>(TotalEstimateBytes));
	Out += TEXT("    \"tables\": {\n");
	AppendMetadataInternedTableJson(Out, TEXT("metadataPrefix"), Prototype.MetadataPrefixTable, true);
	AppendMetadataInternedTableJson(Out, TEXT("optionalData"), Prototype.OptionalDataTable, false);
	Out += TEXT("    },\n");
	Out += TEXT("    \"payloadGroups\": [\n");
	for (int32 GroupIndex = 0; GroupIndex < Prototype.PayloadGroups.Num(); ++GroupIndex)
	{
		const FGroupedShaderPrototypeGroup& Group = Prototype.PayloadGroups[GroupIndex];
		Out += FString::Printf(TEXT("      { \"index\": %d, \"shaderCount\": %d, \"uncompressedSize\": %u, \"storedSize\": %u, \"compressed\": %s, \"file\": "),
			GroupIndex,
			Group.ShaderIndices.Num(),
			Group.UncompressedSize,
			Group.StoredSize,
			Group.bCompressed ? TEXT("true") : TEXT("false"));
		AppendJsonString(Out, Group.File);
		Out += (GroupIndex + 1 == Prototype.PayloadGroups.Num()) ? TEXT(" }\n") : TEXT(" },\n");
	}
	Out += TEXT("    ]\n");
	Out += TEXT("  },\n");
}

struct FPwgslDictionaryEntry
{
	FString Hash;
	uint32 Offset = 0;
	uint32 Size = 0;
	uint32 UseCount = 0;
};

struct FPwgslDictionaryShaderPlacement
{
	int32 EncodedGroupIndex = INDEX_NONE;
	uint32 OffsetInEncodedGroup = 0;
	uint32 EncodedSize = 0;
	bool bFallbackSerializedShader = false;
};

struct FPwgslDictionaryPrototype
{
	bool bValid = false;
	uint32 ChunkSize = 0;
	uint32 MinUses = 0;
	int32 ParsedShaderCount = 0;
	int32 FallbackShaderCount = 0;
	int32 DictionaryEntryCount = 0;
	uint64 SourcePwgslBytes = 0;
	uint64 DictionaryUncompressedBytes = 0;
	uint64 DictionaryStoredBytes = 0;
	bool bDictionaryCompressed = false;
	FString DictionaryFile;
	uint64 EncodedGroupUncompressedBytes = 0;
	uint64 EncodedGroupStoredBytes = 0;
	uint64 MetadataTableStoredBytes = 0;
	uint64 StoredPayloadBytes = 0;
	uint64 PayloadSavingsBytes = 0;
	uint64 IndexBytesEstimate = 0;
	TArray<FGroupedShaderPrototypeGroup> EncodedGroups;
	TArray<FPwgslDictionaryEntry> Entries;
	TArray<FPwgslDictionaryShaderPlacement> ShaderPlacements;
};

static void AppendLiteralDictionaryToken(TArray<uint8>& OutBytes, TConstArrayView<uint8> LiteralBytes)
{
	static constexpr uint8 LiteralTag = 0;
	check(LiteralBytes.Num() <= MAX_uint16);
	OutBytes.Add(LiteralTag);
	AppendUInt16LE(OutBytes, uint16(LiteralBytes.Num()));
	OutBytes.Append(LiteralBytes.GetData(), LiteralBytes.Num());
}

static void AppendDictionaryReferenceToken(TArray<uint8>& OutBytes, uint32 DictionaryIndex)
{
	static constexpr uint8 DictionaryReferenceTag = 1;
	OutBytes.Add(DictionaryReferenceTag);
	AppendUInt32LE(OutBytes, DictionaryIndex);
}

static void AppendFallbackSerializedShaderToken(TArray<uint8>& OutBytes, const TArray<uint8>& ShaderBytes)
{
	static constexpr uint8 FallbackSerializedShaderTag = 2;
	OutBytes.Add(FallbackSerializedShaderTag);
	AppendUInt32LE(OutBytes, ShaderBytes.Num());
	OutBytes.Append(ShaderBytes);
}

static bool BuildPwgslDictionaryPrototype(
	const FString& OutDir,
	const TArray<FLoadedShaderBlob>& Shaders,
	const TArray<FGroupedShaderPrototypeGroup>& SourceGroups,
	const FMetadataInternedPrototype& MetadataInternedPrototype,
	uint32 ChunkSize,
	uint32 MinUses,
	uint64 LegacyStoredBytes,
	FOodleDataCompression::ECompressor OodleCompressor,
	FOodleDataCompression::ECompressionLevel OodleLevel,
	bool bWritePayloads,
	FPwgslDictionaryPrototype& OutPrototype)
{
	OutPrototype = FPwgslDictionaryPrototype();
	OutPrototype.bValid = true;
	OutPrototype.ChunkSize = FMath::Max<uint32>(ChunkSize, 8u);
	OutPrototype.MinUses = FMath::Max<uint32>(MinUses, 2u);
	OutPrototype.MetadataTableStoredBytes = MetadataInternedPrototype.MetadataTableStoredBytes;
	OutPrototype.ShaderPlacements.SetNum(Shaders.Num());

	TMap<FString, uint32> ChunkUseCounts;
	for (const FLoadedShaderBlob& Shader : Shaders)
	{
		const FWebGPUShaderBlobSections& Sections = Shader.Sections;
		if (!Sections.bParsed)
		{
			++OutPrototype.FallbackShaderCount;
			continue;
		}

		++OutPrototype.ParsedShaderCount;
		OutPrototype.SourcePwgslBytes += Sections.WgslPayloadSize;
		const TConstArrayView<uint8> PayloadBytes = SliceBytes(Shader.Bytes, Sections.WgslPayloadOffset, Sections.WgslPayloadSize);
		for (uint32 Offset = 0; Offset + OutPrototype.ChunkSize <= uint32(PayloadBytes.Num()); Offset += OutPrototype.ChunkSize)
		{
			const FString ChunkHash = Sha1String(SliceBytes(PayloadBytes, Offset, OutPrototype.ChunkSize));
			++ChunkUseCounts.FindOrAdd(ChunkHash);
		}
	}

	TMap<FString, int32> DictionaryIndexByHash;
	TArray<uint8> DictionaryBytes;
	for (const FLoadedShaderBlob& Shader : Shaders)
	{
		const FWebGPUShaderBlobSections& Sections = Shader.Sections;
		if (!Sections.bParsed)
		{
			continue;
		}

		const TConstArrayView<uint8> PayloadBytes = SliceBytes(Shader.Bytes, Sections.WgslPayloadOffset, Sections.WgslPayloadSize);
		for (uint32 Offset = 0; Offset + OutPrototype.ChunkSize <= uint32(PayloadBytes.Num()); Offset += OutPrototype.ChunkSize)
		{
			const TConstArrayView<uint8> ChunkBytes = SliceBytes(PayloadBytes, Offset, OutPrototype.ChunkSize);
			const FString ChunkHash = Sha1String(ChunkBytes);
			const uint32* UseCount = ChunkUseCounts.Find(ChunkHash);
			if (!UseCount || *UseCount < OutPrototype.MinUses || DictionaryIndexByHash.Contains(ChunkHash))
			{
				continue;
			}

			const int32 NewIndex = OutPrototype.Entries.Num();
			FPwgslDictionaryEntry& Entry = OutPrototype.Entries.AddDefaulted_GetRef();
			Entry.Hash = ChunkHash;
			Entry.Offset = DictionaryBytes.Num();
			Entry.Size = OutPrototype.ChunkSize;
			Entry.UseCount = *UseCount;
			DictionaryBytes.Append(ChunkBytes.GetData(), ChunkBytes.Num());
			DictionaryIndexByHash.Add(ChunkHash, NewIndex);
		}
	}

	OutPrototype.DictionaryEntryCount = OutPrototype.Entries.Num();
	const FString DictionaryDir = OutDir / TEXT("pwgsl-dictionary");
	const FString EncodedGroupDir = DictionaryDir / TEXT("groups");
	IFileManager::Get().MakeDirectory(*EncodedGroupDir, /*Tree*/ true);

	FCompressedPrototypePayload StoredDictionary;
	if (!CompressPrototypePayload(DictionaryBytes, OodleCompressor, OodleLevel, bWritePayloads, StoredDictionary))
	{
		return false;
	}
	OutPrototype.DictionaryUncompressedBytes = StoredDictionary.UncompressedSize;
	OutPrototype.DictionaryStoredBytes = StoredDictionary.StoredSize;
	OutPrototype.bDictionaryCompressed = StoredDictionary.bCompressed;
	if (StoredDictionary.UncompressedSize > 0)
	{
		OutPrototype.DictionaryFile = FString::Printf(TEXT("pwgsl-dictionary/dictionary.%s"), OutPrototype.bDictionaryCompressed ? TEXT("oodle") : TEXT("raw"));
		if (bWritePayloads)
		{
			const FString DictionaryPath = DictionaryDir / FPaths::GetCleanFilename(OutPrototype.DictionaryFile);
			if (!WriteBytes(DictionaryPath, StoredDictionary.StoredBytes))
			{
				return false;
			}
		}
	}

	OutPrototype.EncodedGroups.Reserve(SourceGroups.Num());
	TArray<uint8> EncodedGroupBytes;
	for (int32 GroupIndex = 0; GroupIndex < SourceGroups.Num(); ++GroupIndex)
	{
		const FGroupedShaderPrototypeGroup& SourceGroup = SourceGroups[GroupIndex];
		FGroupedShaderPrototypeGroup& EncodedGroup = OutPrototype.EncodedGroups.AddDefaulted_GetRef();
		EncodedGroup.ShaderIndices = SourceGroup.ShaderIndices;

		EncodedGroupBytes.Reset();
		for (int32 ShaderIndex : SourceGroup.ShaderIndices)
		{
			const FLoadedShaderBlob& Shader = Shaders[ShaderIndex];
			const FWebGPUShaderBlobSections& Sections = Shader.Sections;
			FPwgslDictionaryShaderPlacement& Placement = OutPrototype.ShaderPlacements[ShaderIndex];
			Placement.EncodedGroupIndex = GroupIndex;
			Placement.OffsetInEncodedGroup = EncodedGroupBytes.Num();

			if (!Sections.bParsed)
			{
				AppendFallbackSerializedShaderToken(EncodedGroupBytes, Shader.Bytes);
				Placement.EncodedSize = EncodedGroupBytes.Num() - Placement.OffsetInEncodedGroup;
				Placement.bFallbackSerializedShader = true;
				continue;
			}

			const TConstArrayView<uint8> PayloadBytes = SliceBytes(Shader.Bytes, Sections.WgslPayloadOffset, Sections.WgslPayloadSize);
			uint32 Offset = 0;
			for (; Offset + OutPrototype.ChunkSize <= uint32(PayloadBytes.Num()); Offset += OutPrototype.ChunkSize)
			{
				const TConstArrayView<uint8> ChunkBytes = SliceBytes(PayloadBytes, Offset, OutPrototype.ChunkSize);
				const FString ChunkHash = Sha1String(ChunkBytes);
				if (const int32* DictionaryIndex = DictionaryIndexByHash.Find(ChunkHash))
				{
					AppendDictionaryReferenceToken(EncodedGroupBytes, uint32(*DictionaryIndex));
				}
				else
				{
					AppendLiteralDictionaryToken(EncodedGroupBytes, ChunkBytes);
				}
			}

			if (Offset < uint32(PayloadBytes.Num()))
			{
				AppendLiteralDictionaryToken(EncodedGroupBytes, SliceBytes(PayloadBytes, Offset, uint32(PayloadBytes.Num()) - Offset));
			}
			Placement.EncodedSize = EncodedGroupBytes.Num() - Placement.OffsetInEncodedGroup;
		}

		FCompressedPrototypePayload StoredGroup;
		if (!CompressPrototypePayload(EncodedGroupBytes, OodleCompressor, OodleLevel, bWritePayloads, StoredGroup))
		{
			return false;
		}

		EncodedGroup.UncompressedSize = StoredGroup.UncompressedSize;
		EncodedGroup.StoredSize = StoredGroup.StoredSize;
		EncodedGroup.bCompressed = StoredGroup.bCompressed;
		EncodedGroup.File = FString::Printf(TEXT("pwgsl-dictionary/groups/encoded-group-%06d.%s"), GroupIndex, EncodedGroup.bCompressed ? TEXT("oodle") : TEXT("raw"));
		OutPrototype.EncodedGroupUncompressedBytes += EncodedGroup.UncompressedSize;
		OutPrototype.EncodedGroupStoredBytes += EncodedGroup.StoredSize;

		if (bWritePayloads)
		{
			const FString EncodedGroupPath = EncodedGroupDir / FPaths::GetCleanFilename(EncodedGroup.File);
			if (!WriteBytes(EncodedGroupPath, StoredGroup.StoredBytes))
			{
				return false;
			}
		}
	}

	OutPrototype.StoredPayloadBytes =
		OutPrototype.MetadataTableStoredBytes +
		OutPrototype.DictionaryStoredBytes +
		OutPrototype.EncodedGroupStoredBytes;
	OutPrototype.PayloadSavingsBytes = LegacyStoredBytes > OutPrototype.StoredPayloadBytes
		? LegacyStoredBytes - OutPrototype.StoredPayloadBytes
		: 0;
	OutPrototype.IndexBytesEstimate =
		MetadataInternedPrototype.IndexBytesEstimate +
		uint64(OutPrototype.Entries.Num()) * 16ull +
		uint64(Shaders.Num()) * 8ull;
	return true;
}

static void AppendPwgslDictionaryPrototypeJson(
	FString& Out,
	const FPwgslDictionaryPrototype& Prototype,
	uint64 LegacyStoredBytes,
	uint64 LegacyHeaderBytes)
{
	const uint64 TotalEstimateBytes = LegacyHeaderBytes + Prototype.IndexBytesEstimate + Prototype.StoredPayloadBytes;

	Out += TEXT("  \"pwgslDictionaryPrototype\": {\n");
	Out += TEXT("    \"encoding\": \"fixed-chunk-pwgsl-dictionary-v0\",\n");
	Out += FString::Printf(TEXT("    \"valid\": %s,\n"), Prototype.bValid ? TEXT("true") : TEXT("false"));
	Out += FString::Printf(TEXT("    \"chunkSize\": %u,\n"), Prototype.ChunkSize);
	Out += FString::Printf(TEXT("    \"minUses\": %u,\n"), Prototype.MinUses);
	Out += FString::Printf(TEXT("    \"parsedShaderCount\": %d,\n"), Prototype.ParsedShaderCount);
	Out += FString::Printf(TEXT("    \"fallbackShaderCount\": %d,\n"), Prototype.FallbackShaderCount);
	Out += FString::Printf(TEXT("    \"sourcePwgslBytes\": %llu,\n"), static_cast<unsigned long long>(Prototype.SourcePwgslBytes));
	Out += FString::Printf(TEXT("    \"dictionaryEntryCount\": %d,\n"), Prototype.DictionaryEntryCount);
	Out += FString::Printf(TEXT("    \"dictionaryUncompressedBytes\": %llu,\n"), static_cast<unsigned long long>(Prototype.DictionaryUncompressedBytes));
	Out += FString::Printf(TEXT("    \"dictionaryStoredBytes\": %llu,\n"), static_cast<unsigned long long>(Prototype.DictionaryStoredBytes));
	Out += FString::Printf(TEXT("    \"dictionaryCompressed\": %s,\n"), Prototype.bDictionaryCompressed ? TEXT("true") : TEXT("false"));
	Out += TEXT("    \"dictionaryFile\": ");
	AppendJsonString(Out, Prototype.DictionaryFile);
	Out += TEXT(",\n");
	Out += FString::Printf(TEXT("    \"encodedGroupUncompressedBytes\": %llu,\n"), static_cast<unsigned long long>(Prototype.EncodedGroupUncompressedBytes));
	Out += FString::Printf(TEXT("    \"encodedGroupStoredBytes\": %llu,\n"), static_cast<unsigned long long>(Prototype.EncodedGroupStoredBytes));
	Out += FString::Printf(TEXT("    \"metadataTableStoredBytes\": %llu,\n"), static_cast<unsigned long long>(Prototype.MetadataTableStoredBytes));
	Out += FString::Printf(TEXT("    \"storedPayloadBytes\": %llu,\n"), static_cast<unsigned long long>(Prototype.StoredPayloadBytes));
	Out += FString::Printf(TEXT("    \"payloadSavingsBytes\": %lld,\n"), static_cast<long long>(LegacyStoredBytes) - static_cast<long long>(Prototype.StoredPayloadBytes));
	Out += FString::Printf(TEXT("    \"payloadRatio\": %.6f,\n"), LegacyStoredBytes > 0 ? double(Prototype.StoredPayloadBytes) / double(LegacyStoredBytes) : 0.0);
	Out += FString::Printf(TEXT("    \"indexBytesEstimate\": %llu,\n"), static_cast<unsigned long long>(Prototype.IndexBytesEstimate));
	Out += FString::Printf(TEXT("    \"totalEstimateBytes\": %llu,\n"), static_cast<unsigned long long>(TotalEstimateBytes));
	Out += TEXT("    \"encodedGroups\": [\n");
	for (int32 GroupIndex = 0; GroupIndex < Prototype.EncodedGroups.Num(); ++GroupIndex)
	{
		const FGroupedShaderPrototypeGroup& Group = Prototype.EncodedGroups[GroupIndex];
		Out += FString::Printf(TEXT("      { \"index\": %d, \"shaderCount\": %d, \"uncompressedSize\": %u, \"storedSize\": %u, \"compressed\": %s, \"file\": "),
			GroupIndex,
			Group.ShaderIndices.Num(),
			Group.UncompressedSize,
			Group.StoredSize,
			Group.bCompressed ? TEXT("true") : TEXT("false"));
		AppendJsonString(Out, Group.File);
		Out += (GroupIndex + 1 == Prototype.EncodedGroups.Num()) ? TEXT(" }\n") : TEXT(" },\n");
	}
	Out += TEXT("    ]\n");
	Out += TEXT("  },\n");
}

static TArray<uint8> BuildRedirectShaderBlob(
	const FLoadedShaderBlob& Shader,
	const FGroupedShaderPrototypeGroup& Group,
	const FGroupedShaderPlacement& Placement,
	const FString& RelativeGroupPath)
{
	TArray<uint8> Blob;
	FTCHARToUTF8 RelativePathUtf8(*RelativeGroupPath);
	Blob.Reserve(10 * int32(sizeof(uint32)) + RelativePathUtf8.Length());
	AppendUInt32LE(Blob, WebGPUShaderArchiveRedirectMagic);
	AppendUInt32LE(Blob, WebGPUShaderArchiveRedirectVersion);
	AppendUInt32LE(Blob, Group.bCompressed ? WebGPUShaderArchiveRedirectFlagCompressedGroup : 0u);
	AppendUInt32LE(Blob, Shader.UncompressedSize);
	AppendUInt32LE(Blob, uint32(Placement.GroupIndex));
	AppendUInt32LE(Blob, Placement.OffsetInGroup);
	AppendUInt32LE(Blob, Shader.UncompressedSize);
	AppendUInt32LE(Blob, Group.UncompressedSize);
	AppendUInt32LE(Blob, Group.StoredSize);
	AppendUInt32LE(Blob, uint32(RelativePathUtf8.Length()));
	Blob.Append(reinterpret_cast<const uint8*>(RelativePathUtf8.Get()), RelativePathUtf8.Length());
	return Blob;
}

static FString NormalizeRedirectRelativePath(FString Path)
{
	Path.ReplaceInline(TEXT("\\"), TEXT("/"));
	while (Path.StartsWith(TEXT("/")))
	{
		Path.RightChopInline(1);
	}
	return Path;
}

static bool ParseRedirectShaderBlob(TConstArrayView<uint8> Blob, FParsedRedirectShaderBlob& OutRedirect)
{
	int32 Offset = 0;
	uint32 Magic = 0;
	uint32 Version = 0;
	uint32 PathBytes = 0;
	if (!ReadUInt32LEAndAdvance(Blob, Offset, Magic) || Magic != WebGPUShaderArchiveRedirectMagic)
	{
		return false;
	}
	if (!ReadUInt32LEAndAdvance(Blob, Offset, Version) ||
		Version != WebGPUShaderArchiveRedirectVersion ||
		!ReadUInt32LEAndAdvance(Blob, Offset, OutRedirect.Flags) ||
		!ReadUInt32LEAndAdvance(Blob, Offset, OutRedirect.OriginalSerializedSize) ||
		!ReadUInt32LEAndAdvance(Blob, Offset, OutRedirect.GroupIndex) ||
		!ReadUInt32LEAndAdvance(Blob, Offset, OutRedirect.OffsetInGroup) ||
		!ReadUInt32LEAndAdvance(Blob, Offset, OutRedirect.SizeInGroup) ||
		!ReadUInt32LEAndAdvance(Blob, Offset, OutRedirect.GroupUncompressedSize) ||
		!ReadUInt32LEAndAdvance(Blob, Offset, OutRedirect.GroupStoredSize) ||
		!ReadUInt32LEAndAdvance(Blob, Offset, PathBytes))
	{
		UE_LOG(LogWebGPUShaderArchiveTool, Error, TEXT("Malformed redirect shader blob header"));
		return false;
	}

	if ((OutRedirect.Flags & ~WebGPUShaderArchiveRedirectFlagCompressedGroup) != 0)
	{
		UE_LOG(LogWebGPUShaderArchiveTool, Error, TEXT("Unsupported redirect shader blob flags 0x%08x"), OutRedirect.Flags);
		return false;
	}

	if (PathBytes == 0 || PathBytes > 4096 || Offset + int32(PathBytes) != Blob.Num())
	{
		UE_LOG(LogWebGPUShaderArchiveTool, Error,
			TEXT("Malformed redirect shader blob path: pathBytes=%u blobSize=%d"),
			PathBytes,
			Blob.Num());
		return false;
	}

	FUTF8ToTCHAR PathUtf8(reinterpret_cast<const ANSICHAR*>(Blob.GetData() + Offset), PathBytes);
	OutRedirect.RelativeGroupPath = NormalizeRedirectRelativePath(FString(PathUtf8.Length(), PathUtf8.Get()));
	return true;
}

static bool ResolveRedirectGroupPathForValidation(
	const FParsedRedirectShaderBlob& Redirect,
	const FString& GroupRoot,
	const FString& ContentRoot,
	const FString& RedirectContentPrefix,
	FString& OutGroupPath)
{
	if (!ContentRoot.IsEmpty())
	{
		OutGroupPath = ContentRoot / Redirect.RelativeGroupPath;
		FPaths::NormalizeFilename(OutGroupPath);
		return true;
	}

	if (GroupRoot.IsEmpty())
	{
		UE_LOG(LogWebGPUShaderArchiveTool, Error, TEXT("Redirect validation requires -GroupRoot=<prototype out dir> or -ContentRoot=<staged Content dir>"));
		return false;
	}

	FString RelativePath = Redirect.RelativeGroupPath;
	const FString Prefix = NormalizeRedirectRelativePath(RedirectContentPrefix);
	if (!Prefix.IsEmpty())
	{
		const FString PrefixWithSlash = Prefix + TEXT("/");
		if (RelativePath.StartsWith(PrefixWithSlash))
		{
			RelativePath.RightChopInline(PrefixWithSlash.Len());
		}
		else if (RelativePath == Prefix)
		{
			RelativePath.Empty();
		}
	}

	if (!RelativePath.StartsWith(TEXT("groups/")))
	{
		const int32 GroupsOffset = RelativePath.Find(TEXT("groups/"), ESearchCase::CaseSensitive, ESearchDir::FromStart);
		if (GroupsOffset != INDEX_NONE)
		{
			RelativePath = RelativePath.Mid(GroupsOffset);
		}
	}

	if (RelativePath.IsEmpty() ||
		RelativePath.StartsWith(TEXT("../")) ||
		RelativePath.Contains(TEXT("/../")) ||
		RelativePath.Contains(TEXT(":")))
	{
		UE_LOG(LogWebGPUShaderArchiveTool, Error, TEXT("Invalid redirect group relative path '%s'"), *Redirect.RelativeGroupPath);
		return false;
	}

	OutGroupPath = GroupRoot / RelativePath;
	FPaths::NormalizeFilename(OutGroupPath);
	return true;
}

static bool LoadRedirectGroupForValidation(
	const FParsedRedirectShaderBlob& Redirect,
	const FString& GroupPath,
	TMap<FString, TSharedPtr<TArray<uint8>>>& GroupCache,
	TSharedPtr<TArray<uint8>>& OutGroupBytes)
{
	const FString CacheKey = FString::Printf(TEXT("%s|%u|%u|%u"),
		*GroupPath,
		Redirect.Flags,
		Redirect.GroupStoredSize,
		Redirect.GroupUncompressedSize);
	if (TSharedPtr<TArray<uint8>>* Existing = GroupCache.Find(CacheKey))
	{
		OutGroupBytes = *Existing;
		return true;
	}

	TArray<uint8> StoredGroupBytes;
	if (!FFileHelper::LoadFileToArray(StoredGroupBytes, *GroupPath))
	{
		UE_LOG(LogWebGPUShaderArchiveTool, Error, TEXT("Failed to load redirect group '%s'"), *GroupPath);
		return false;
	}

	if (uint32(StoredGroupBytes.Num()) != Redirect.GroupStoredSize)
	{
		UE_LOG(LogWebGPUShaderArchiveTool, Error,
			TEXT("Redirect group '%s' stored size mismatch: file=%d expected=%u"),
			*GroupPath,
			StoredGroupBytes.Num(),
			Redirect.GroupStoredSize);
		return false;
	}

	TSharedPtr<TArray<uint8>> LoadedGroup = MakeShared<TArray<uint8>>();
	if ((Redirect.Flags & WebGPUShaderArchiveRedirectFlagCompressedGroup) != 0)
	{
		LoadedGroup->SetNumUninitialized(Redirect.GroupUncompressedSize);
		ShaderCodeArchive::DecompressShaderWithOodle(
			LoadedGroup->GetData(),
			Redirect.GroupUncompressedSize,
			StoredGroupBytes.GetData(),
			StoredGroupBytes.Num());
	}
	else
	{
		if (uint32(StoredGroupBytes.Num()) != Redirect.GroupUncompressedSize)
		{
			UE_LOG(LogWebGPUShaderArchiveTool, Error,
				TEXT("Redirect raw group '%s' uncompressed size mismatch: file=%d expected=%u"),
				*GroupPath,
				StoredGroupBytes.Num(),
				Redirect.GroupUncompressedSize);
			return false;
		}
		*LoadedGroup = MoveTemp(StoredGroupBytes);
	}

	if (uint32(LoadedGroup->Num()) != Redirect.GroupUncompressedSize)
	{
		UE_LOG(LogWebGPUShaderArchiveTool, Error,
			TEXT("Redirect group '%s' decompressed size mismatch: got=%d expected=%u"),
			*GroupPath,
			LoadedGroup->Num(),
			Redirect.GroupUncompressedSize);
		return false;
	}

	GroupCache.Add(CacheKey, LoadedGroup);
	OutGroupBytes = MoveTemp(LoadedGroup);
	return true;
}

static bool ValidateRedirectArchiveInternal(
	const FString& SourceArchivePath,
	const FString& RedirectArchivePath,
	const FString& GroupRoot,
	const FString& ContentRoot,
	const FString& RedirectContentPrefix)
{
	FSerializedShaderArchive SourceArchive;
	int64 SourceCodeStartOffset = 0;
	if (!LoadArchive(SourceArchivePath, SourceArchive, SourceCodeStartOffset))
	{
		return false;
	}

	FSerializedShaderArchive RedirectArchive;
	int64 RedirectCodeStartOffset = 0;
	if (!LoadArchive(RedirectArchivePath, RedirectArchive, RedirectCodeStartOffset))
	{
		return false;
	}

	if (SourceArchive.GetNumShaders() != RedirectArchive.GetNumShaders() ||
		SourceArchive.GetNumShaderMaps() != RedirectArchive.GetNumShaderMaps())
	{
		UE_LOG(LogWebGPUShaderArchiveTool, Error,
			TEXT("Redirect archive shape mismatch: source shaders/maps=%d/%d redirect shaders/maps=%d/%d"),
			SourceArchive.GetNumShaders(),
			SourceArchive.GetNumShaderMaps(),
			RedirectArchive.GetNumShaders(),
			RedirectArchive.GetNumShaderMaps());
		return false;
	}

	const TArrayView<const FShaderHash> SourceHashes = SourceArchive.GetShaderHashes();
	const TArrayView<const FShaderHash> RedirectHashes = RedirectArchive.GetShaderHashes();
	if (SourceHashes.Num() != RedirectHashes.Num())
	{
		UE_LOG(LogWebGPUShaderArchiveTool, Error,
			TEXT("Redirect archive hash table size mismatch: source=%d redirect=%d"),
			SourceHashes.Num(),
			RedirectHashes.Num());
		return false;
	}
	for (int32 HashIndex = 0; HashIndex < SourceHashes.Num(); ++HashIndex)
	{
		if (SourceHashes[HashIndex] != RedirectHashes[HashIndex])
		{
			UE_LOG(LogWebGPUShaderArchiveTool, Error, TEXT("Redirect archive shader hash mismatch at index %d"), HashIndex);
			return false;
		}
	}

	TArray<FLoadedShaderBlob> SourceShaders;
	if (!LoadAllShaderBlobs(SourceArchivePath, SourceArchive, SourceCodeStartOffset, SourceShaders))
	{
		return false;
	}

	TArray<FLoadedShaderBlob> RedirectShaders;
	if (!LoadAllShaderBlobs(RedirectArchivePath, RedirectArchive, RedirectCodeStartOffset, RedirectShaders))
	{
		return false;
	}

	TMap<FString, TSharedPtr<TArray<uint8>>> GroupCache;
	for (int32 ShaderIndex = 0; ShaderIndex < SourceShaders.Num(); ++ShaderIndex)
	{
		const FLoadedShaderBlob& SourceShader = SourceShaders[ShaderIndex];
		const FLoadedShaderBlob& RedirectShader = RedirectShaders[ShaderIndex];
		if (SourceShader.Frequency != RedirectShader.Frequency)
		{
			UE_LOG(LogWebGPUShaderArchiveTool, Error,
				TEXT("Redirect shader frequency mismatch at shader %d: source=%u redirect=%u"),
				ShaderIndex,
				uint32(SourceShader.Frequency),
				uint32(RedirectShader.Frequency));
			return false;
		}

		FParsedRedirectShaderBlob Redirect;
		if (!ParseRedirectShaderBlob(RedirectShader.Bytes, Redirect))
		{
			UE_LOG(LogWebGPUShaderArchiveTool, Error, TEXT("Shader %d is not a valid redirect blob"), ShaderIndex);
			return false;
		}

		if (Redirect.OriginalSerializedSize != uint32(SourceShader.Bytes.Num()) ||
			Redirect.SizeInGroup != uint32(SourceShader.Bytes.Num()) ||
			Redirect.SizeInGroup == 0 ||
			Redirect.OffsetInGroup > Redirect.GroupUncompressedSize ||
			Redirect.SizeInGroup > Redirect.GroupUncompressedSize - Redirect.OffsetInGroup)
		{
			UE_LOG(LogWebGPUShaderArchiveTool, Error,
				TEXT("Redirect shader %d has invalid bounds: original=%u source=%d offset=%u size=%u groupSize=%u"),
				ShaderIndex,
				Redirect.OriginalSerializedSize,
				SourceShader.Bytes.Num(),
				Redirect.OffsetInGroup,
				Redirect.SizeInGroup,
				Redirect.GroupUncompressedSize);
			return false;
		}

		FString GroupPath;
		if (!ResolveRedirectGroupPathForValidation(Redirect, GroupRoot, ContentRoot, RedirectContentPrefix, GroupPath))
		{
			return false;
		}

		TSharedPtr<TArray<uint8>> GroupBytes;
		if (!LoadRedirectGroupForValidation(Redirect, GroupPath, GroupCache, GroupBytes) || !GroupBytes.IsValid())
		{
			return false;
		}

		const uint8* ReconstructedShader = GroupBytes->GetData() + Redirect.OffsetInGroup;
		if (FMemory::Memcmp(ReconstructedShader, SourceShader.Bytes.GetData(), SourceShader.Bytes.Num()) != 0)
		{
			UE_LOG(LogWebGPUShaderArchiveTool, Error,
				TEXT("Redirect shader %d reconstructed bytes differ from source hash=%s group=%u path=%s"),
				ShaderIndex,
				*SourceShader.Hash,
				Redirect.GroupIndex,
				*Redirect.RelativeGroupPath);
			return false;
		}
	}

	UE_LOG(LogWebGPUShaderArchiveTool, Display,
		TEXT("Validated redirect archive '%s': shaders=%d groupsLoaded=%d"),
		*RedirectArchivePath,
		SourceShaders.Num(),
		GroupCache.Num());
	return true;
}

static bool WriteRedirectArchive(
	const FString& RedirectArchivePath,
	const FString& RedirectContentPrefix,
	const FSerializedShaderArchive& SourceArchive,
	const TArray<FLoadedShaderBlob>& Shaders,
	const TArray<FGroupedShaderPrototypeGroup>& Groups,
	const TArray<FGroupedShaderPlacement>& ShaderPlacements,
	uint64& OutRedirectPayloadBytes,
	uint64& OutRedirectArchiveBytes)
{
	if (RedirectArchivePath.IsEmpty())
	{
		OutRedirectPayloadBytes = 0;
		OutRedirectArchiveBytes = 0;
		return true;
	}

	FSerializedShaderArchive RedirectArchive = SourceArchive;
	TArray<TArray<uint8>> RedirectBlobs;
	RedirectBlobs.SetNum(Shaders.Num());
	OutRedirectPayloadBytes = 0;

	for (const FLoadedShaderBlob& Shader : Shaders)
	{
		const FGroupedShaderPlacement& Placement = ShaderPlacements[Shader.Index];
		if (!Groups.IsValidIndex(Placement.GroupIndex))
		{
			UE_LOG(LogWebGPUShaderArchiveTool, Error, TEXT("Shader %d has invalid redirect group index %d"), Shader.Index, Placement.GroupIndex);
			return false;
		}

		const FGroupedShaderPrototypeGroup& Group = Groups[Placement.GroupIndex];
		FString RelativeGroupPath = RedirectContentPrefix / Group.File;
		RelativeGroupPath = NormalizeRedirectRelativePath(RelativeGroupPath);

		RedirectBlobs[Shader.Index] = BuildRedirectShaderBlob(Shader, Group, Placement, RelativeGroupPath);

		FShaderCodeEntry& Entry = RedirectArchive.ShaderEntries[Shader.Index];
		Entry.TagAndFrequency = 0;
		Entry.SetFrequency(Shader.Frequency);
		Entry.Size = RedirectBlobs[Shader.Index].Num();
		Entry.SetUncompressedSize(RedirectBlobs[Shader.Index].Num());
		OutRedirectPayloadBytes += Entry.Size;
	}

	for (FShaderMapEntry& ShaderMapEntry : RedirectArchive.ShaderMapEntries)
	{
		ShaderMapEntry.FirstPreloadIndex = 0;
		ShaderMapEntry.NumPreloadEntries = 0;
	}
	RedirectArchive.Finalize();

	TArray<uint8> ArchiveBytes;
	FMemoryWriter Writer(ArchiveBytes);
	if (!FSerializedShaderArchive::SerializeHeaderVersion(Writer))
	{
		UE_LOG(LogWebGPUShaderArchiveTool, Error, TEXT("Failed to serialize redirect archive version"));
		return false;
	}
	Writer << RedirectArchive;

	for (const TArray<uint8>& RedirectBlob : RedirectBlobs)
	{
		ArchiveBytes.Append(RedirectBlob);
	}

	OutRedirectArchiveBytes = ArchiveBytes.Num();
	if (!WriteArchiveBytes(RedirectArchivePath, ArchiveBytes))
	{
		return false;
	}

	UE_LOG(LogWebGPUShaderArchiveTool, Display,
		TEXT("Wrote redirect archive '%s': payload=%llu total=%llu"),
		*RedirectArchivePath,
		static_cast<unsigned long long>(OutRedirectPayloadBytes),
		static_cast<unsigned long long>(OutRedirectArchiveBytes));
	return true;
}

static bool BuildGroupedPrototype()
{
	FString ArchivePath;
	FString OutDir;
	if (!FParse::Value(FCommandLine::Get(), TEXT("Archive="), ArchivePath, /*bShouldStopOnSeparator*/ false) || ArchivePath.IsEmpty())
	{
		UE_LOG(LogWebGPUShaderArchiveTool, Error, TEXT("Missing -Archive=<ShaderArchive-*.ushaderbytecode>"));
		return false;
	}
	if (!FParse::Value(FCommandLine::Get(), TEXT("Out="), OutDir, /*bShouldStopOnSeparator*/ false) || OutDir.IsEmpty())
	{
		UE_LOG(LogWebGPUShaderArchiveTool, Error, TEXT("Missing -Out=<output directory>"));
		return false;
	}

	FString Policy = TEXT("IoStore");
	FParse::Value(FCommandLine::Get(), TEXT("Policy="), Policy);

	uint64 MaxGroupSize = 16ull * 1024ull * 1024ull;
	FString MaxGroupSizeText;
	if (FParse::Value(FCommandLine::Get(), TEXT("MaxGroupSize="), MaxGroupSizeText, /*bShouldStopOnSeparator*/ false))
	{
		if (!ParseByteSize(MaxGroupSizeText, MaxGroupSize) || MaxGroupSize == 0)
		{
			UE_LOG(LogWebGPUShaderArchiveTool, Error, TEXT("Invalid -MaxGroupSize='%s'"), *MaxGroupSizeText);
			return false;
		}
	}

	FString CompressorText;
	FString LevelText;
	FParse::Value(FCommandLine::Get(), TEXT("Compressor="), CompressorText, /*bShouldStopOnSeparator*/ false);
	FParse::Value(FCommandLine::Get(), TEXT("Level="), LevelText, /*bShouldStopOnSeparator*/ false);

	FOodleDataCompression::ECompressor OodleCompressor;
	FOodleDataCompression::ECompressionLevel OodleLevel;
	if (!ParseOodleCompressor(CompressorText, OodleCompressor))
	{
		UE_LOG(LogWebGPUShaderArchiveTool, Error, TEXT("Invalid -Compressor='%s'"), *CompressorText);
		return false;
	}
	if (!ParseOodleLevel(LevelText, OodleLevel))
	{
		UE_LOG(LogWebGPUShaderArchiveTool, Error, TEXT("Invalid -Level='%s'"), *LevelText);
		return false;
	}

	const bool bWritePayloads = !FParse::Param(FCommandLine::Get(), TEXT("NoPayloads"));

	FString RedirectArchivePath;
	FParse::Value(FCommandLine::Get(), TEXT("RedirectArchive="), RedirectArchivePath, /*bShouldStopOnSeparator*/ false);

	FString RedirectContentPrefix;
	FParse::Value(FCommandLine::Get(), TEXT("RedirectContentPrefix="), RedirectContentPrefix, /*bShouldStopOnSeparator*/ false);
	if (RedirectArchivePath.IsEmpty() == false && RedirectContentPrefix.IsEmpty())
	{
		RedirectContentPrefix = TEXT("WebGPUShaderGroups") / FPaths::GetBaseFilename(ArchivePath);
	}
	RedirectContentPrefix.ReplaceInline(TEXT("\\"), TEXT("/"));
	while (RedirectContentPrefix.StartsWith(TEXT("/")))
	{
		RedirectContentPrefix.RightChopInline(1);
	}

	if (!RedirectArchivePath.IsEmpty() && !bWritePayloads)
	{
		UE_LOG(LogWebGPUShaderArchiveTool, Error, TEXT("-RedirectArchive requires grouped payload files; remove -NoPayloads for this run."));
		return false;
	}
	const bool bSkipRedirectValidation = FParse::Param(FCommandLine::Get(), TEXT("SkipRedirectValidation"));
	const bool bRuntimeOnly = FParse::Param(FCommandLine::Get(), TEXT("RuntimeOnly"));

	uint32 DictionaryChunkSize = 4096;
	uint32 DictionaryMinUses = 3;
	FString DictionaryChunkSizeText;
	if (FParse::Value(FCommandLine::Get(), TEXT("DictionaryChunkSize="), DictionaryChunkSizeText, /*bShouldStopOnSeparator*/ false))
	{
		const uint64 Parsed = FCString::Strtoui64(*DictionaryChunkSizeText, nullptr, 10);
		if (Parsed < 8 || Parsed > MAX_uint16)
		{
			UE_LOG(LogWebGPUShaderArchiveTool, Error, TEXT("Invalid -DictionaryChunkSize='%s'. Expected 8..65535."), *DictionaryChunkSizeText);
			return false;
		}
		DictionaryChunkSize = uint32(Parsed);
	}

	FString DictionaryMinUsesText;
	if (FParse::Value(FCommandLine::Get(), TEXT("DictionaryMinUses="), DictionaryMinUsesText, /*bShouldStopOnSeparator*/ false))
	{
		const uint64 Parsed = FCString::Strtoui64(*DictionaryMinUsesText, nullptr, 10);
		if (Parsed < 2 || Parsed > MAX_uint16)
		{
			UE_LOG(LogWebGPUShaderArchiveTool, Error, TEXT("Invalid -DictionaryMinUses='%s'. Expected 2..65535."), *DictionaryMinUsesText);
			return false;
		}
		DictionaryMinUses = uint32(Parsed);
	}

	FSerializedShaderArchive SerializedShaders;
	int64 CodeStartOffset = 0;
	if (!LoadArchive(ArchivePath, SerializedShaders, CodeStartOffset))
	{
		return false;
	}

	TArray<FLoadedShaderBlob> Shaders;
	if (!LoadAllShaderBlobs(ArchivePath, SerializedShaders, CodeStartOffset, Shaders))
	{
		return false;
	}
	const FWebGPUShaderBlobSectionStats SectionStats = BuildSectionStats(Shaders);

	const FString FormatName = GetArchiveFormatName(ArchivePath);
	TArray<FGroupedShaderPrototypeGroup> Groups;
	uint32 PrototypeHeaderBytes = 0;
	if (!BuildPrototypeGroupsFromPolicy(Policy, MaxGroupSize, FName(*FormatName), SerializedShaders, Shaders, Groups, PrototypeHeaderBytes))
	{
		return false;
	}
	const bool bPrototypeHeaderBytesEstimated = PrototypeHeaderBytes == 0;
	if (bPrototypeHeaderBytesEstimated)
	{
		const uint64 EstimatedBytes = uint64(CodeStartOffset) + uint64(Groups.Num()) * 32ull + uint64(Shaders.Num()) * 8ull;
		PrototypeHeaderBytes = uint32(FMath::Min<uint64>(EstimatedBytes, MAX_uint32));
	}

	IFileManager::Get().MakeDirectory(*OutDir, /*Tree*/ true);
	const FString GroupDir = OutDir / TEXT("groups");
	TArray<FGroupedShaderPlacement> ShaderPlacements;
	uint64 GroupedStoredBytes = 0;
	uint64 GroupedUncompressedBytes = 0;
	if (!CompressAndWritePrototypeGroups(GroupDir, Shaders, OodleCompressor, OodleLevel, bWritePayloads, Groups, ShaderPlacements, GroupedStoredBytes, GroupedUncompressedBytes))
	{
		return false;
	}

	uint64 RedirectPayloadBytes = 0;
	uint64 RedirectArchiveBytes = 0;
	if (!WriteRedirectArchive(RedirectArchivePath, RedirectContentPrefix, SerializedShaders, Shaders, Groups, ShaderPlacements, RedirectPayloadBytes, RedirectArchiveBytes))
	{
		return false;
	}
	bool bRedirectValidated = false;
	if (!RedirectArchivePath.IsEmpty() && !bSkipRedirectValidation)
	{
		if (!ValidateRedirectArchiveInternal(ArchivePath, RedirectArchivePath, OutDir, FString(), RedirectContentPrefix))
		{
			return false;
		}
		bRedirectValidated = true;
	}

	uint64 LegacyStoredBytes = 0;
	uint64 LegacyUncompressedBytes = 0;
	for (const FLoadedShaderBlob& Shader : Shaders)
	{
		LegacyStoredBytes += Shader.StoredSize;
		LegacyUncompressedBytes += Shader.UncompressedSize;
	}

	FMetadataInternedPrototype MetadataInternedPrototype;
	if (!bRuntimeOnly)
	{
		if (!BuildMetadataInternedPrototype(OutDir, Shaders, Groups, OodleCompressor, OodleLevel, bWritePayloads, MetadataInternedPrototype))
		{
			return false;
		}
	}

	FPwgslDictionaryPrototype PwgslDictionaryPrototype;
	if (!bRuntimeOnly)
	{
		if (!BuildPwgslDictionaryPrototype(
			OutDir,
			Shaders,
			Groups,
			MetadataInternedPrototype,
			DictionaryChunkSize,
			DictionaryMinUses,
			LegacyStoredBytes,
			OodleCompressor,
			OodleLevel,
			bWritePayloads,
			PwgslDictionaryPrototype))
		{
			return false;
		}
	}

	const TCHAR* CompressorName = FOodleDataCompression::ECompressorToString(OodleCompressor);
	const TCHAR* LevelName = FOodleDataCompression::ECompressionLevelToString(OodleLevel);

	FString Manifest;
	Manifest += TEXT("{\n");
	Manifest += TEXT("  \"format\": \"webgpu-grouped-shader-prototype-v1\",\n");
	Manifest += TEXT("  \"archive\": ");
	AppendJsonString(Manifest, FPaths::ConvertRelativePathToFull(ArchivePath));
	Manifest += TEXT(",\n");
	Manifest += TEXT("  \"archiveFormat\": ");
	AppendJsonString(Manifest, FormatName);
	Manifest += TEXT(",\n");
	Manifest += TEXT("  \"policy\": ");
	AppendJsonString(Manifest, Policy);
	Manifest += TEXT(",\n");
	Manifest += FString::Printf(TEXT("  \"maxGroupSize\": %llu,\n"), static_cast<unsigned long long>(MaxGroupSize));
	Manifest += TEXT("  \"compressor\": ");
	AppendJsonString(Manifest, CompressorName);
	Manifest += TEXT(",\n");
	Manifest += TEXT("  \"level\": ");
	AppendJsonString(Manifest, LevelName);
	Manifest += TEXT(",\n");
	Manifest += FString::Printf(TEXT("  \"runtimeOnly\": %s,\n"), bRuntimeOnly ? TEXT("true") : TEXT("false"));
	Manifest += FString::Printf(TEXT("  \"shaderCount\": %d,\n"), Shaders.Num());
	Manifest += FString::Printf(TEXT("  \"groupCount\": %d,\n"), Groups.Num());
	Manifest += FString::Printf(TEXT("  \"legacyHeaderBytes\": %lld,\n"), CodeStartOffset);
	Manifest += FString::Printf(TEXT("  \"prototypeHeaderBytes\": %u,\n"), PrototypeHeaderBytes);
	Manifest += FString::Printf(TEXT("  \"prototypeHeaderBytesEstimated\": %s,\n"), bPrototypeHeaderBytesEstimated ? TEXT("true") : TEXT("false"));
	Manifest += FString::Printf(TEXT("  \"legacyStoredShaderBytes\": %llu,\n"), static_cast<unsigned long long>(LegacyStoredBytes));
	Manifest += FString::Printf(TEXT("  \"legacyUncompressedShaderBytes\": %llu,\n"), static_cast<unsigned long long>(LegacyUncompressedBytes));
	Manifest += FString::Printf(TEXT("  \"groupedStoredShaderBytes\": %llu,\n"), static_cast<unsigned long long>(GroupedStoredBytes));
	Manifest += FString::Printf(TEXT("  \"groupedUncompressedShaderBytes\": %llu,\n"), static_cast<unsigned long long>(GroupedUncompressedBytes));
	Manifest += FString::Printf(TEXT("  \"groupedPayloadSavingsBytes\": %lld,\n"), static_cast<long long>(LegacyStoredBytes) - static_cast<long long>(GroupedStoredBytes));
	Manifest += FString::Printf(TEXT("  \"groupedPayloadRatio\": %.6f,\n"), LegacyStoredBytes > 0 ? double(GroupedStoredBytes) / double(LegacyStoredBytes) : 0.0);
	Manifest += TEXT("  \"redirectArchive\": ");
	AppendJsonString(Manifest, RedirectArchivePath.IsEmpty() ? FString() : FPaths::ConvertRelativePathToFull(RedirectArchivePath));
	Manifest += TEXT(",\n");
	Manifest += TEXT("  \"redirectContentPrefix\": ");
	AppendJsonString(Manifest, RedirectContentPrefix);
	Manifest += TEXT(",\n");
	Manifest += FString::Printf(TEXT("  \"redirectPayloadBytes\": %llu,\n"), static_cast<unsigned long long>(RedirectPayloadBytes));
	Manifest += FString::Printf(TEXT("  \"redirectArchiveBytes\": %llu,\n"), static_cast<unsigned long long>(RedirectArchiveBytes));
	Manifest += FString::Printf(TEXT("  \"redirectValidated\": %s,\n"), bRedirectValidated ? TEXT("true") : TEXT("false"));
	AppendSectionStatsJson(Manifest, SectionStats);
	if (!bRuntimeOnly)
	{
		AppendMetadataInternedPrototypeJson(Manifest, MetadataInternedPrototype, LegacyStoredBytes, uint64(CodeStartOffset));
		AppendPwgslDictionaryPrototypeJson(Manifest, PwgslDictionaryPrototype, LegacyStoredBytes, uint64(CodeStartOffset));
	}
	Manifest += TEXT("  \"groups\": [\n");
	for (int32 GroupIndex = 0; GroupIndex < Groups.Num(); ++GroupIndex)
	{
		const FGroupedShaderPrototypeGroup& Group = Groups[GroupIndex];
		Manifest += FString::Printf(TEXT("    { \"index\": %d, \"shaderCount\": %d, \"uncompressedSize\": %u, \"storedSize\": %u, \"compressed\": %s, \"file\": "),
			GroupIndex,
			Group.ShaderIndices.Num(),
			Group.UncompressedSize,
			Group.StoredSize,
			Group.bCompressed ? TEXT("true") : TEXT("false"));
		AppendJsonString(Manifest, Group.File);
		Manifest += (GroupIndex + 1 == Groups.Num()) ? TEXT(" }\n") : TEXT(" },\n");
	}
	Manifest += TEXT("  ],\n");
	Manifest += TEXT("  \"shaders\": [\n");
	for (int32 ShaderIndex = 0; ShaderIndex < Shaders.Num(); ++ShaderIndex)
	{
		const FLoadedShaderBlob& Shader = Shaders[ShaderIndex];
		const FWebGPUShaderBlobSections& Sections = Shader.Sections;
		const FGroupedShaderPlacement& Placement = ShaderPlacements[ShaderIndex];
		const FMetadataInternedShaderPlacement EmptyInternedPlacement;
		const FMetadataInternedShaderPlacement& InternedPlacement = MetadataInternedPrototype.ShaderPlacements.IsValidIndex(ShaderIndex)
			? MetadataInternedPrototype.ShaderPlacements[ShaderIndex]
			: EmptyInternedPlacement;
		Manifest += FString::Printf(TEXT("    { \"index\": %d, \"hash\": "), Shader.Index);
		AppendJsonString(Manifest, Shader.Hash);
		Manifest += FString::Printf(TEXT(", \"frequency\": %u, \"legacyOffset\": %llu, \"legacyStoredSize\": %u, \"uncompressedSize\": %u, \"foundPackedWgsl\": %s, \"wgslPayloadOffset\": %u, \"wgslPayloadSize\": %u, \"wgslUnpackedSize\": %u, \"wgslLengthBucket\": %u, \"wgslTokenPrefixCrc\": %u, \"sectionParsed\": %s, \"metadataPrefixSize\": %u, \"pwgslPayloadSize\": %u, \"optionalDataSize\": %u, \"metadataPrefixSha1\": "),
			uint32(Shader.Frequency),
			static_cast<unsigned long long>(Shader.LegacyOffset),
			Shader.StoredSize,
			Shader.UncompressedSize,
			Shader.bFoundPackedWgsl ? TEXT("true") : TEXT("false"),
			Shader.WgslPayloadOffset,
			Shader.WgslPayloadSize,
			Shader.WgslUnpackedSize,
			Shader.WgslLengthBucket,
			Shader.WgslTokenPrefixCrc,
			Sections.bParsed ? TEXT("true") : TEXT("false"),
			Sections.MetadataPrefixSize,
			Sections.WgslPayloadSize,
			Sections.OptionalDataSize);
		AppendJsonString(Manifest, Sections.MetadataPrefixSha1);
		Manifest += TEXT(", \"pwgslPayloadSha1\": ");
		AppendJsonString(Manifest, Sections.WgslPayloadSha1);
		Manifest += TEXT(", \"optionalDataSha1\": ");
		AppendJsonString(Manifest, Sections.OptionalDataSha1);
		Manifest += FString::Printf(TEXT(", \"group\": %d, \"offsetInGroup\": %u, \"metadataPrefixIndex\": %d, \"optionalDataIndex\": %d, \"internedPayloadGroup\": %d, \"internedOffsetInGroup\": %u, \"internedPayloadSize\": %u, \"internedFallbackSerializedShader\": %s }%s\n"),
			Placement.GroupIndex,
			Placement.OffsetInGroup,
			InternedPlacement.MetadataPrefixIndex,
			InternedPlacement.OptionalDataIndex,
			InternedPlacement.PayloadGroupIndex,
			InternedPlacement.OffsetInPayloadGroup,
			InternedPlacement.PayloadSize,
			InternedPlacement.bFallbackSerializedShader ? TEXT("true") : TEXT("false"),
			ShaderIndex + 1 == Shaders.Num() ? TEXT("") : TEXT(","));
	}
	Manifest += TEXT("  ]\n");
	Manifest += TEXT("}\n");

	const FString ManifestPath = OutDir / TEXT("manifest.json");
	if (!FFileHelper::SaveStringToFile(Manifest, *ManifestPath))
	{
		UE_LOG(LogWebGPUShaderArchiveTool, Error, TEXT("Failed to write '%s'"), *ManifestPath);
		return false;
	}

	UE_LOG(LogWebGPUShaderArchiveTool, Display,
		TEXT("Built grouped prototype '%s': policy=%s groups=%d legacyPayload=%llu groupedPayload=%llu savings=%lld"),
		*OutDir,
		*Policy,
		Groups.Num(),
		static_cast<unsigned long long>(LegacyStoredBytes),
		static_cast<unsigned long long>(GroupedStoredBytes),
		static_cast<long long>(LegacyStoredBytes) - static_cast<long long>(GroupedStoredBytes));
	return true;
}

static bool ValidateRedirectArchive()
{
	FString SourceArchivePath;
	FParse::Value(FCommandLine::Get(), TEXT("SourceArchive="), SourceArchivePath, /*bShouldStopOnSeparator*/ false);
	if (SourceArchivePath.IsEmpty())
	{
		FParse::Value(FCommandLine::Get(), TEXT("Archive="), SourceArchivePath, /*bShouldStopOnSeparator*/ false);
	}
	if (SourceArchivePath.IsEmpty())
	{
		UE_LOG(LogWebGPUShaderArchiveTool, Error, TEXT("Missing -SourceArchive=<original ShaderArchive-*.ushaderbytecode> or -Archive=<original archive>"));
		return false;
	}

	FString RedirectArchivePath;
	FParse::Value(FCommandLine::Get(), TEXT("RedirectArchive="), RedirectArchivePath, /*bShouldStopOnSeparator*/ false);
	if (RedirectArchivePath.IsEmpty())
	{
		UE_LOG(LogWebGPUShaderArchiveTool, Error, TEXT("Missing -RedirectArchive=<generated redirect archive>"));
		return false;
	}

	FString GroupRoot;
	FParse::Value(FCommandLine::Get(), TEXT("GroupRoot="), GroupRoot, /*bShouldStopOnSeparator*/ false);

	FString ContentRoot;
	FParse::Value(FCommandLine::Get(), TEXT("ContentRoot="), ContentRoot, /*bShouldStopOnSeparator*/ false);

	FString RedirectContentPrefix;
	FParse::Value(FCommandLine::Get(), TEXT("RedirectContentPrefix="), RedirectContentPrefix, /*bShouldStopOnSeparator*/ false);
	RedirectContentPrefix = NormalizeRedirectRelativePath(RedirectContentPrefix);

	if (GroupRoot.IsEmpty() && ContentRoot.IsEmpty())
	{
		const FString CandidateGroupRoot = FPaths::GetPath(RedirectArchivePath) / FPaths::GetBaseFilename(SourceArchivePath);
		if (IFileManager::Get().DirectoryExists(*CandidateGroupRoot))
		{
			GroupRoot = CandidateGroupRoot;
		}
	}

	return ValidateRedirectArchiveInternal(SourceArchivePath, RedirectArchivePath, GroupRoot, ContentRoot, RedirectContentPrefix);
}

static bool ExtractDecompressed()
{
	FString ArchivePath;
	FString OutDir;
	if (!FParse::Value(FCommandLine::Get(), TEXT("Archive="), ArchivePath, /*bShouldStopOnSeparator*/ false) || ArchivePath.IsEmpty())
	{
		UE_LOG(LogWebGPUShaderArchiveTool, Error, TEXT("Missing -Archive=<ShaderArchive-*.ushaderbytecode>"));
		return false;
	}
	if (!FParse::Value(FCommandLine::Get(), TEXT("Out="), OutDir, /*bShouldStopOnSeparator*/ false) || OutDir.IsEmpty())
	{
		UE_LOG(LogWebGPUShaderArchiveTool, Error, TEXT("Missing -Out=<output directory>"));
		return false;
	}

	FSerializedShaderArchive SerializedShaders;
	int64 CodeStartOffset = 0;
	if (!LoadArchive(ArchivePath, SerializedShaders, CodeStartOffset))
	{
		return false;
	}

	IFileManager& FileManager = IFileManager::Get();
	FileManager.MakeDirectory(*OutDir, /*Tree*/ true);
	const FString BlobDir = OutDir / TEXT("blobs");
	FileManager.MakeDirectory(*BlobDir, /*Tree*/ true);

	FString Manifest;
	Manifest += TEXT("{\n");
	Manifest += TEXT("  \"format\": \"webgpu-shader-archive-extract-v1\",\n");
	Manifest += TEXT("  \"archive\": ");
	AppendJsonString(Manifest, FPaths::ConvertRelativePathToFull(ArchivePath));
	Manifest += TEXT(",\n");
	Manifest += FString::Printf(TEXT("  \"codeStartOffset\": %lld,\n"), CodeStartOffset);
	Manifest += FString::Printf(TEXT("  \"shaderCount\": %d,\n"), SerializedShaders.GetNumShaders());
	Manifest += FString::Printf(TEXT("  \"shaderMapCount\": %d,\n"), SerializedShaders.GetNumShaderMaps());
	Manifest += TEXT("  \"blobs\": [\n");

	TArray<uint8> CompressedPayload;
	TArray<uint8> DecompressedShader;
	uint64 TotalStoredBytes = 0;
	uint64 TotalUncompressedBytes = 0;

	const TArrayView<const FShaderCodeEntry> ShaderEntries = SerializedShaders.GetShaderEntries();
	const TArrayView<const FShaderHash> ShaderHashes = SerializedShaders.GetShaderHashes();
	for (int32 ShaderIndex = 0; ShaderIndex < ShaderEntries.Num(); ++ShaderIndex)
	{
		const FShaderCodeEntry& Entry = ShaderEntries[ShaderIndex];
		if (!ReadCompressedPayload(ArchivePath, Entry, CodeStartOffset, CompressedPayload))
		{
			return false;
		}

		DecompressedShader.Reset();
		FSerializedShaderArchive::DecompressShaderEntryAndAppend(Entry, CompressedPayload.GetData(), DecompressedShader);

		const FString HashString = ShaderHashes.IsValidIndex(ShaderIndex)
			? ShaderHashes[ShaderIndex].ToString()
			: FString::Printf(TEXT("missinghash%06d"), ShaderIndex);
		const FString BlobName = FString::Printf(TEXT("%06d-%s.ushaderblob"), ShaderIndex, *HashString);
		const FString BlobPath = BlobDir / BlobName;
		if (!WriteBytes(BlobPath, DecompressedShader))
		{
			return false;
		}

		TotalStoredBytes += Entry.Size;
		TotalUncompressedBytes += Entry.GetUncompressedSize();

		Manifest += FString::Printf(TEXT("    { \"index\": %d, \"hash\": "), ShaderIndex);
		AppendJsonString(Manifest, HashString);
		Manifest += FString::Printf(TEXT(", \"frequency\": %u, \"archiveOffset\": %llu, \"storedSize\": %u, \"uncompressedSize\": %u, \"isCompressed\": %s, \"file\": "),
			uint32(Entry.GetFrequency()),
			static_cast<unsigned long long>(Entry.Offset),
			Entry.Size,
			Entry.GetUncompressedSize(),
			Entry.IsCompressed() ? TEXT("true") : TEXT("false"));
		AppendJsonString(Manifest, FString(TEXT("blobs/")) + BlobName);
		Manifest += (ShaderIndex + 1 == ShaderEntries.Num()) ? TEXT(" }\n") : TEXT(" },\n");
	}

	Manifest += TEXT("  ],\n");
	Manifest += FString::Printf(TEXT("  \"totalStoredBytes\": %llu,\n"), static_cast<unsigned long long>(TotalStoredBytes));
	Manifest += FString::Printf(TEXT("  \"totalUncompressedBytes\": %llu\n"), static_cast<unsigned long long>(TotalUncompressedBytes));
	Manifest += TEXT("}\n");

	const FString ManifestPath = OutDir / TEXT("manifest.json");
	if (!FFileHelper::SaveStringToFile(Manifest, *ManifestPath))
	{
		UE_LOG(LogWebGPUShaderArchiveTool, Error, TEXT("Failed to write '%s'"), *ManifestPath);
		return false;
	}

	UE_LOG(LogWebGPUShaderArchiveTool, Display,
		TEXT("Extracted %d shaders to '%s' (stored=%llu uncompressed=%llu manifest=%s)"),
		ShaderEntries.Num(),
		*BlobDir,
		static_cast<unsigned long long>(TotalStoredBytes),
		static_cast<unsigned long long>(TotalUncompressedBytes),
		*ManifestPath);
	return true;
}

static int32 WebGPUShaderArchiveToolMain()
{
	GPrintLogTimes = ELogTimes::None;

	const bool bHelp = FParse::Param(FCommandLine::Get(), TEXT("help")) || FParse::Param(FCommandLine::Get(), TEXT("?"));
	if (bHelp)
	{
		UE_LOG(LogWebGPUShaderArchiveTool, Display,
			TEXT("Usage:\n")
			TEXT("  WebGPUShaderArchiveTool -Mode=ExtractDecompressed -Archive=<ShaderArchive-*.ushaderbytecode> -Out=<dir>\n")
			TEXT("  WebGPUShaderArchiveTool -Mode=BuildGroupedPrototype -Archive=<ShaderArchive-*.ushaderbytecode> -Out=<dir> [-Policy=IoStore|Input|Frequency|Similarity|Solid] [-MaxGroupSize=16MiB] [-Compressor=Mermaid|Kraken|Leviathan|Selkie] [-Level=Normal|Optimal1|...] [-DictionaryChunkSize=4096] [-DictionaryMinUses=3] [-RedirectArchive=<path>] [-RedirectContentPrefix=WebGPUShaderGroups/<archive>] [-RuntimeOnly] [-SkipRedirectValidation] [-NoPayloads]\n")
			TEXT("  WebGPUShaderArchiveTool -Mode=ValidateRedirectArchive -SourceArchive=<original archive> -RedirectArchive=<redirect archive> [-GroupRoot=<prototype out dir> | -ContentRoot=<staged Content dir>] [-RedirectContentPrefix=WebGPUShaderGroups/<archive>]\n")
			TEXT("    BuildGroupedPrototype also reports experimental metadata-interned and fixed-chunk pWGSL dictionary payloads using the same policy and Oodle settings.\n"));
		return 0;
	}

	FString Mode = TEXT("ExtractDecompressed");
	FParse::Value(FCommandLine::Get(), TEXT("Mode="), Mode);
	if (Mode.Equals(TEXT("ExtractDecompressed"), ESearchCase::IgnoreCase))
	{
		return ExtractDecompressed() ? 0 : 1;
	}
	if (Mode.Equals(TEXT("BuildGroupedPrototype"), ESearchCase::IgnoreCase))
	{
		return BuildGroupedPrototype() ? 0 : 1;
	}
	if (Mode.Equals(TEXT("ValidateRedirectArchive"), ESearchCase::IgnoreCase))
	{
		return ValidateRedirectArchive() ? 0 : 1;
	}

	UE_LOG(LogWebGPUShaderArchiveTool, Error, TEXT("Unknown -Mode='%s'"), *Mode);
	return 1;
}
} // namespace UE::SimplyStream::ShaderArchiveTool

INT32_MAIN_INT32_ARGC_TCHAR_ARGV()
{
	FTaskTagScope Scope(ETaskTag::EGameThread);

	if (int32 Ret = GEngineLoop.PreInit(ArgC, ArgV, TEXT("-stdout")); Ret != 0)
	{
		return Ret;
	}

	ON_SCOPE_EXIT
	{
		LLM(FLowLevelMemTracker::Get().UpdateStatsPerFrame());
		RequestEngineExit(TEXT("Exiting"));
		FEngineLoop::AppPreExit();
		FModuleManager::Get().UnloadModulesAtShutdown();
		FEngineLoop::AppExit();
	};

	return UE::SimplyStream::ShaderArchiveTool::WebGPUShaderArchiveToolMain();
}
