// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "OpenGLDrvPrivate.h"
#include "SimplyStreamOpenGL.h"
THIRD_PARTY_INCLUDES_START
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
THIRD_PARTY_INCLUDES_END
#include "SimplyStreamJavaScriptFx.h"

#include "OpenGLShaders.h"
#include "archive.h"

//#define MAIN_THREAD(...)
//#define EM_ASM(...)
//#define EM_ASM_INT(...) 0

#ifndef PLATFORM_SIMPLYSTREAM_ENABLE_ANTIALIAS
#define PLATFORM_SIMPLYSTREAM_ENABLE_ANTIALIAS 1
#endif

DEFINE_LOG_CATEGORY_STATIC(LogSimplyStreamOpenGL, Log, All);

void FSimplyStreamOpenGL::ProcessExtensions(const FString& ExtensionsStrings)
{
#if 0
#if 1
	FOpenGLES::ProcessExtensions(ExtensionsStrings);
	GRHIVendorId = 0xFFFF;
	bSupportsASTC = false; //ExtensionsString.Contains(TEXT("GL_KHR_texture_compression_astc_ldr"));
	bSupportsASTCHDR = bSupportsASTC; // && ExtensionsString.Contains(TEXT("GL_KHR_texture_compression_astc_hdr"));
	bSupportsCopyImage = false;// ExtensionsString.Contains(TEXT("GL_ARB_copy_image"));
	bSupportsTextureFilterAnisotropic = true;// ExtensionsString.Contains(TEXT("GL_EXT_texture_filter_anisotropic"));
	bSupportsDrawBuffersBlend = false;//ExtensionsString.Contains(TEXT("GL_ARB_draw_buffers_blend"));

	bSupportsMapBuffer = ExtensionsStrings.Contains(TEXT("GL_OES_mapbuffer"));
	//bSupportsVertexHalfFloat = false;
	bSupportsShaderFramebufferFetch = ExtensionsStrings.Contains(TEXT("GL_EXT_shader_framebuffer_fetch")) || ExtensionsStrings.Contains(TEXT("GL_NV_shader_framebuffer_fetch"));
	//bRequiresUEShaderFramebufferFetchDef = ExtensionsStrings.Contains(TEXT("GL_EXT_shader_framebuffer_fetch"));
	bSupportsDXT = true;
	bSupportsETC2 = true;
	bSupportsNVFrameBufferBlit = true;
	//EM_ASM({console.log('bSupportsMapBuffer', $0, 'bSupportsShaderFramebufferFetch', $1, 'bRequiresUEShaderFramebufferFetchDef', $2);}, bSupportsMapBuffer, bSupportsShaderFramebufferFetch, bRequiresUEShaderFramebufferFetchDef);

	//for (auto& v : ExtensionsStrings)
	//	EM_ASM({ console.log('ext', UTF8ToString($0)); }, TCHAR_TO_UTF8(*v));

	bSupportsColorBufferHalfFloat = true;
#else
	FOpenGLES::ProcessExtensions(ExtensionsStrings);
	GRHIVendorId = 0xFFFF;
	bSupportsASTC = false; //ExtensionsString.Contains(TEXT("GL_KHR_texture_compression_astc_ldr"));
	bSupportsASTCHDR = bSupportsASTC; // && ExtensionsString.Contains(TEXT("GL_KHR_texture_compression_astc_hdr"));
	bSupportsCopyImage = false;// ExtensionsString.Contains(TEXT("GL_ARB_copy_image"));
	bSupportsTextureFilterAnisotropic = ExtensionsStrings.Contains(TEXT("GL_EXT_texture_filter_anisotropic")); // should usually be true
	bSupportsDrawBuffersBlend = true; // should be true

	bSupportsMapBuffer = ExtensionsStrings.Contains(TEXT("GL_OES_mapbuffer"));
	//bSupportsVertexHalfFloat = false;
	bSupportsShaderFramebufferFetch = ExtensionsStrings.Contains(TEXT("GL_EXT_shader_framebuffer_fetch")) || ExtensionsStrings.Contains(TEXT("GL_NV_shader_framebuffer_fetch"));
	//bRequiresUEShaderFramebufferFetchDef = ExtensionsStrings.Contains(TEXT("GL_EXT_shader_framebuffer_fetch"));
	bSupportsDXT = true;
	bSupportsETC2 = true;
	bSupportsNVFrameBufferBlit = true;
	//EM_ASM({console.log('bSupportsMapBuffer', $0, 'bSupportsShaderFramebufferFetch', $1, 'bRequiresUEShaderFramebufferFetchDef', $2);}, bSupportsMapBuffer, bSupportsShaderFramebufferFetch, bRequiresUEShaderFramebufferFetchDef);

	//for (auto& v : ExtensionsStrings)
	//	EM_ASM({ console.log('ext', UTF8ToString($0)); }, TCHAR_TO_UTF8(*v));

	bSupportsColorBufferFloat = ExtensionsStrings.Contains(TEXT("EXT_color_buffer_float")); // should usually be true
	bSupportsColorBufferHalfFloat = ExtensionsStrings.Contains(TEXT("EXT_color_buffer_half_float")); // should usually be true
	//FOpenGL::GetShaderPrecisionFormat(GL_FRAGMENT_SHADER, GL_LOW_FLOAT, Range, &ShaderLowPrecision);
	//FOpenGL::GetShaderPrecisionFormat(GL_FRAGMENT_SHADER, GL_MEDIUM_FLOAT, Range, &ShaderMediumPrecision);
	//FOpenGL::GetShaderPrecisionFormat(GL_FRAGMENT_SHADER, GL_HIGH_FLOAT, Range, &ShaderHighPrecision);
	//UE_LOG(LogRHI, Log, TEXT("Fragment shader lowp precision: %d"), ShaderLowPrecision);
	//UE_LOG(LogRHI, Log, TEXT("Fragment shader mediump precision: %d"), ShaderMediumPrecision);
	//UE_LOG(LogRHI, Log, TEXT("Fragment shader highp precision: %d"), ShaderHighPrecision);
#endif
#endif
}

void FSimplyStreamOpenGL::PE_GetCurrentOpenGLShaderDeviceCapabilities(FOpenGLShaderDeviceCapabilities& Capabilities)
{
	//	Capabilities.TargetPlatform = EOpenGLShaderTargetPlatform::OGLSTP_SimplyStream;
	Capabilities.TargetPlatform = EOpenGLShaderTargetPlatform::OGLSTP_Unknown; // jic
	//Capabilities.bSupportsShaderTextureLod = FSimplyStreamOpenGL::SupportsShaderTextureLod();
	Capabilities.bSupportsShaderFramebufferFetch = FSimplyStreamOpenGL::SupportsShaderFramebufferFetch();
	Capabilities.bRequiresARMShaderFramebufferFetchDepthStencilUndef = FOpenGL::RequiresARMShaderFramebufferFetchDepthStencilUndef();
	//Capabilities.bRequiresDontEmitPrecisionForTextureSamplers = FOpenGL::RequiresDontEmitPrecisionForTextureSamplers();
	//Capabilities.bRequiresTextureCubeLodEXTToTextureCubeLodDefine = FOpenGL::RequiresTextureCubeLodEXTToTextureCubeLodDefine();
	Capabilities.MaxVaryingVectors = FOpenGL::GetMaxVaryingVectors();
	//Capabilities.bRequiresTexture2DPrecisionHack = FOpenGL::RequiresTexture2DPrecisionHack();
	//Capabilities.bRequiresRoundFunctionHack = FOpenGL::RequiresRoundFunctionHack();
}
char* strstr2(const char* string, const char* substring)
{
	const char* a, * b;

	/* First scan quickly through the two strings looking for a
	 * single-character match.  When it's found, then compare the
	 * rest of the substring.
	 */

	b = substring;

	if (*b == 0)
	{
		return (char*)string;
	}

	for (; *string != 0; string += 1)
	{
		if (*string != *b)
		{
			continue;
		}

		a = string;

		while (1)
		{
			if (*b == 0)
			{
				return (char*)string;
			}
			if (*a++ != *b++)
			{
				break;
			}
		}

		b = substring;
	}

	return NULL;
}
namespace
{
	inline const ANSICHAR* CStringEndOfLine2(const ANSICHAR* Text)
	{
		const ANSICHAR* LineEnd = FCStringAnsi::Strchr(Text, '\n');
		if (nullptr == LineEnd)
		{
			LineEnd = Text + FCStringAnsi::Strlen(Text);
		}
		return LineEnd;
	}

	inline bool CStringIsBlankLine2(const ANSICHAR* Text)
	{
		while (!FCharAnsi::IsLinebreak(*Text))
		{
			if (!FCharAnsi::IsWhitespace(*Text))
			{
				return false;
			}
			++Text;
		}
		return true;
	}

	inline int CStringCountOccurances2(TArray<ANSICHAR>& Source, const ANSICHAR* TargetString)
	{
		int32 TargetLen = FCStringAnsi::Strlen(TargetString);
		int Count = 0;
		int32 FoundIndex = 0;
		for (const ANSICHAR* FoundPointer = FCStringAnsi::Strstr(Source.GetData(), TargetString);
			nullptr != FoundPointer;
			FoundPointer = FCStringAnsi::Strstr(Source.GetData() + FoundIndex, TargetString))
		{
			FoundIndex = FoundPointer - Source.GetData();
			FoundIndex += TargetLen;
			Count++;
		}
		return Count;
	}

	inline bool MoveHashLines2(TArray<ANSICHAR>& Dest, TArray<ANSICHAR>& Source)
	{
		// Walk through the lines to find the first non-# line...
		const ANSICHAR* LineStart = Source.GetData();
		for (bool FoundNonHashLine = false; !FoundNonHashLine;)
		{
			const ANSICHAR* LineEnd = CStringEndOfLine2(LineStart);
			if (LineStart[0] != '#' && !CStringIsBlankLine2(LineStart))
			{
				FoundNonHashLine = true;
			}
			else if (LineEnd[0] == '\n')
			{
				LineStart = LineEnd + 1;
			}
			else
			{
				LineStart = LineEnd;
			}
		}
		// Copy the hash lines over, if we found any. And delete from
		// the source.
		if (LineStart > Source.GetData())
		{
			int32 LineLength = LineStart - Source.GetData();
			if (Dest.Num() > 0)
			{
				Dest.Insert(Source.GetData(), LineLength, Dest.Num() - 1);
			}
			else
			{
				Dest.Append(Source.GetData(), LineLength);
				Dest.Append("", 1);
			}
			if (Dest.Last(1) != '\n')
			{
				Dest.Insert("\n", 1, Dest.Num() - 1);
			}
			Source.RemoveAt(0, LineStart - Source.GetData());
			return true;
		}
		return false;
	}
}
bool FSimplyStreamOpenGL::PE_GLSLToDeviceCompatibleGLSL(FAnsiCharArray& GlslCodeOriginal, const FString& ShaderName, GLenum TypeEnum, const FOpenGLShaderDeviceCapabilities& Capabilities, FAnsiCharArray& GlslCode) {
#if 0
	std::string t = TCHAR_TO_UTF8(*ShaderName);
	//wonder_log::info("Compiling shader {}", t);
	std::string code = TCHAR_TO_UTF8(*FString(GlslCodeOriginal.GetData()));
	//wonder_log::info("Shader code {}", code);

	const char* ES300Version = "#version 300 es";
	const ANSICHAR* ES310Version = "#version 310 es";
	uint32_t len = FCStringAnsi::Strlen(GlslCodeOriginal.GetData());
	const bool bES3 = (FCStringAnsi::Strstr(GlslCodeOriginal.GetData(), ES300Version) != nullptr);
	const bool bES31 = (FCStringAnsi::Strstr(GlslCodeOriginal.GetData(), ES310Version) != nullptr);
	//EM_ASM({ console.log('found ES300Version:', $0, $1, $4, $5, 'length:', $2, UTF8ToString($3)); }, (uint32_t)bES3, (uint32_t)bES3_2, len, GlslCodeOriginal.GetData(), (uint32_t)bES3_3, (uint32_t)bES3_4);

	FAnsiCharArray GlslCodeAfterExtensions;
	const ANSICHAR* GlslPlaceHolderAfterExtensions = "// end extensions";
	bool bGlslCodeHasExtensions = (FCStringAnsi::Strstr(GlslCodeOriginal.GetData(), GlslPlaceHolderAfterExtensions) != nullptr);

	PE_AppendCString(GlslCode, "#version 300 es\n");
	//PE_ReplaceCString(GlslCodeOriginal, "#version 100", "");
	//fix these, should be done at compile time, also why is it emitting version 100?
	PE_ReplaceCString(GlslCodeOriginal, "#version 300 es", ""); //should not be needed
	PE_ReplaceCString(GlslCodeOriginal, "#version 310 es", ""); //should not be needed
	PE_ReplaceCString(GlslCodeOriginal, "#version 320 es", ""); //should not be needed
	PE_ReplaceCString(GlslCodeOriginal, "#version 100 es", "");    //should not be needed

	PE_AppendCString(GlslCode, "#define INTERFACE_LOCATION(Pos) layout(location=Pos) \n");
	PE_AppendCString(GlslCode, "#define INTERFACE_BLOCK(Pos, Interp, Modifiers, Semantic, PreType, PostType) layout(location=Pos) Modifiers Semantic { PreType PostType; }\n");
	PE_AppendCString(GlslCode, "uint bitfieldRevers2(uint n) { n = ((n >> 1) & 0x55555555u) | ((n & 0x55555555u) << 1); n = ((n >> 2) & 0x33333333u) | ((n & 0x33333333u) << 2); n = ((n >> 4) & 0x0f0f0f0fu) | ((n & 0x0f0f0f0fu) << 4); n = ((n >> 8) & 0x00ff00ffu) | ((n & 0x00ff00ffu) << 8); n = ((n >> 16) & 0xffffu) | ((n & 0xffffu) << 16); return n;}\n");

	if (TypeEnum == GL_VERTEX_SHADER)
	{
		PE_AppendCString(GlslCode, ("#define texture2D texture \n"
			"#define texture2DProj textureProj \n"
			"#define texture2DLod textureLod \n"
			"#define texture2DLodEXT textureLod \n"
			"#define texture2DProjLod textureProjLod \n"
			"#define textureCube texture \n"
			"#define textureCubeLod textureLod \n"
			"#define textureCubeLodEXT textureLod \n"
			"#define texture3D texture \n"
			"#define texture3DProj textureProj \n"
			"#define texture3DLod textureLod \n"));

		PE_ReplaceCString(GlslCodeOriginal, "attribute", "in");
		PE_ReplaceCString(GlslCodeOriginal, "varying", "out");
		PE_ReplaceCString(GlslCodeOriginal, "var_", "in_");
	}
	else if (TypeEnum == GL_FRAGMENT_SHADER)
	{
		// #extension directives have to come before any non-# directives. Because
		// we add non-# stuff below and the #extension directives
		// get added to the incoming shader source we move any # directives
		// to be right after the #version to ensure they are always correct.
		//MoveHashLines(StrOutSource, OriginalShaderSource);
		const bool bEmitTextureExternal = (FCStringAnsi::Strstr(GlslCodeOriginal.GetData(), "samplerExternalOES") != nullptr);
		if (bEmitTextureExternal) {
			PE_ReplaceCString(GlslCodeOriginal, "// Uses samplerExternalOES", "");
			MoveHashLines2(GlslCode, GlslCodeOriginal);
			PE_AppendCString(GlslCode, "#extension GL_OES_EGL_image_external_essl3 : require\n");
		}


		PE_AppendCString(GlslCode, ("#define texture2D texture \n"
			"#define texture2DProj textureProj \n"
			"#define texture2DLod textureLod \n"
			"#define texture2DLodEXT textureLod \n"
			"#define texture2DProjLod textureProjLod \n"
			"#define textureCube texture \n"
			"#define textureCubeLod textureLod \n"
			"#define textureCubeLodEXT textureLod \n"
			"#define texture3D texture \n"
			"#define texture3DProj textureProj \n"
			"#define texture3DLod textureLod \n"
			"#define texture3DProjLod textureProjLod \n"
			"\n"
));

		PE_ReplaceCString(GlslCodeOriginal, "varying", "in");
		//PE_ReplaceCString(GlslCodeOriginal, "var_", "in_");
		PE_ReplaceCString(GlslCodeOriginal, "#extension GL_EXT_shader_texture_lod : enable", "");
#if 0
		PE_AppendCString(GlslCodeAfterExtensions, (
			"#define gl_FragColor out_FragColor \n"
			"#ifdef EXT_shader_framebuffer_fetch_enabled \n"
			"inout mediump vec4 out_FragColor; \n"
			"#else \n"
			"layout(location=0) out mediump vec4 out_FragColor; \n"
			"#endif \n"));
#endif
		//PE_ReplaceCString(GlslCodeOriginal, "#extension GL_OES_standard_derivatives : enable", "");
	}

	PE_ReplaceCString(GlslCodeOriginal, "bitfieldReverse", "bitfieldRevers2");


	if (ShaderName.IsEmpty() == false)
	{
		PE_AppendCString(GlslCode, "// ");
		PE_AppendCString(GlslCode, TCHAR_TO_ANSI(ShaderName.GetCharArray().GetData()));
		PE_AppendCString(GlslCode, "\n");
	}

	PE_AppendCString(GlslCode, "#define HLSLCC_DX11ClipSpace 1 \n");

	// Append the possibly edited shader to the one we will compile.
	// This is to make it easier to debug as we can see the whole
	// shader source.
	PE_AppendCString(GlslCode, "\n\n");
	PE_AppendCString(GlslCode, GlslCodeOriginal.GetData());

	if (bGlslCodeHasExtensions && GlslCodeAfterExtensions.Num() > 0)
	{
		//EM_ASM({ console.log('adding extensions') });
		// the initial code has an #extension chunk. replace the placeholder line
		PE_ReplaceCString(GlslCode, GlslPlaceHolderAfterExtensions, GlslCodeAfterExtensions.GetData());
	}
#endif
	return true;
}

void FSimplyStreamOpenGL::PE_SetupTextureFormat(void(*SetupTextureFormat)(EPixelFormat, const FOpenGLTextureFormat&))
{
	//might need to be
	//SetupTextureFormat(PF_R8G8B8A8_UINT, FOpenGLTextureFormat(GL_RGBA8, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, false, false));
	//but for now keep as
	SetupTextureFormat(PF_R8G8B8A8_UINT, FOpenGLTextureFormat(GL_RGBA8UI, GL_RGBA8UI, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, false, false));
	SetupTextureFormat(PF_FloatRGB, FOpenGLTextureFormat(GL_RGBA16F, GL_RGBA16F, GL_RGB, GL_HALF_FLOAT, false, false));
	//SetupTextureFormat(PF_FloatRGBA, FOpenGLTextureFormat(GL_RGBA, GL_RGBA, GL_RGBA, GL_HALF_FLOAT_OES, false, false));
	SetupTextureFormat(PF_R8G8B8A8_SNORM,	FOpenGLTextureFormat( GL_RGBA8,					GL_RGBA8,				GL_RGBA,		GL_UNSIGNED_BYTE,					false,	false));
	SetupTextureFormat(PF_R32_UINT, FOpenGLTextureFormat(GL_R32UI, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, false, false));
	SetupTextureFormat(PF_R32_SINT, FOpenGLTextureFormat(GL_R32I, GL_R32I, GL_RED_INTEGER, GL_INT, false, false));
	SetupTextureFormat(PF_R16_UINT, FOpenGLTextureFormat(GL_R16UI, GL_R16UI, GL_RED_INTEGER, GL_UNSIGNED_SHORT, false, false));
	SetupTextureFormat(PF_R16_SINT, FOpenGLTextureFormat(GL_R16I, GL_R16I, GL_RED_INTEGER, GL_SHORT, false, false));
#if 0
	SetupTextureFormat(PF_R8G8B8A8_UINT, FOpenGLTextureFormat(GL_RGBA8UI, GL_RGBA8UI, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, false, false));
	SetupTextureFormat(PF_FloatRGB, FOpenGLTextureFormat(GL_RGB16F, GL_RGB16F, GL_RGB, GL_HALF_FLOAT, false, false));
	SetupTextureFormat(PF_FloatRGBA, FOpenGLTextureFormat(GL_RGBA, GL_RGBA, GL_RGBA, GL_HALF_FLOAT_OES, false, false));
	SetupTextureFormat(PF_R8G8B8A8_SNORM, FOpenGLTextureFormat(GL_RGBA8, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, false, false));
	SetupTextureFormat(PF_R32_UINT, FOpenGLTextureFormat(GL_R32UI, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, false, false));
	SetupTextureFormat(PF_R32_SINT, FOpenGLTextureFormat(GL_R32I, GL_R32I, GL_RED_INTEGER, GL_INT, false, false));
	SetupTextureFormat(PF_R16_UINT, FOpenGLTextureFormat(GL_R16UI, GL_R16UI, GL_RED_INTEGER, GL_UNSIGNED_SHORT, false, false));
	SetupTextureFormat(PF_R16_SINT, FOpenGLTextureFormat(GL_R16I, GL_R16I, GL_RED_INTEGER, GL_SHORT, false, false));
	SetupTextureFormat(PF_R32G32_UINT, FOpenGLTextureFormat(GL_RG32UI, GL_RG32UI, GL_RG_INTEGER, GL_UNSIGNED_INT, false, false));
	SetupTextureFormat(PF_R32_UINT, FOpenGLTextureFormat(GL_R32UI, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, false, false));
	SetupTextureFormat(PF_R32_SINT, FOpenGLTextureFormat(GL_R32I, GL_R32I, GL_RED_INTEGER, GL_INT, false, false));
	SetupTextureFormat(PF_R16_UINT, FOpenGLTextureFormat(GL_R16UI, GL_R16UI, GL_RED_INTEGER, GL_UNSIGNED_SHORT, false, false));
	SetupTextureFormat(PF_R16_SINT, FOpenGLTextureFormat(GL_R16I, GL_R16I, GL_RED_INTEGER, GL_SHORT, false, false));
	SetupTextureFormat(PF_R8_UINT, FOpenGLTextureFormat(GL_R8UI, GL_R8UI, GL_RED_INTEGER, GL_UNSIGNED_BYTE, false, false));
	SetupTextureFormat(PF_R8G8, FOpenGLTextureFormat(GL_RG8, GL_RG8, GL_RG, GL_UNSIGNED_BYTE, false, false));
#endif
#if 0
	if (FOpenGL::SupportsBASIS())
	{
		SetupTextureFormat(PF_ASTC_4x4, FOpenGLTextureFormat(GL_COMPRESSED_RGBA_ASTC_4x4_KHR, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR, GL_RGBA, GL_UNSIGNED_BYTE, true, false));
		SetupTextureFormat(PF_ASTC_6x6, FOpenGLTextureFormat(GL_COMPRESSED_RGBA_ASTC_6x6_KHR, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR, GL_RGBA, GL_UNSIGNED_BYTE, true, false));
		SetupTextureFormat(PF_ASTC_8x8, FOpenGLTextureFormat(GL_COMPRESSED_RGBA_ASTC_8x8_KHR, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR, GL_RGBA, GL_UNSIGNED_BYTE, true, false));
		SetupTextureFormat(PF_ASTC_10x10, FOpenGLTextureFormat(GL_COMPRESSED_RGBA_ASTC_10x10_KHR, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR, GL_RGBA, GL_UNSIGNED_BYTE, true, false));
		SetupTextureFormat(PF_ASTC_12x12, FOpenGLTextureFormat(GL_COMPRESSED_RGBA_ASTC_12x12_KHR, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR, GL_RGBA, GL_UNSIGNED_BYTE, true, false));
	}
#endif
}

struct FPlatformOpenGLContext
{
	GLuint			ViewportFramebuffer;
	GLuint			VertexArrayObject;	// one has to be generated and set for each context (OpenGL 3.2 Core requirements) technically not required for WebGL but avoids warning
	EMSCRIPTEN_WEBGL_CONTEXT_HANDLE Context;

	FPlatformOpenGLContext()
		:Context(NULL),
		ViewportFramebuffer(0)
	{
	}
};

extern "C" int GSystemResolution_ResX();
extern "C" int GSystemResolution_ResY();

struct FPlatformOpenGLDevice
{
	FPlatformOpenGLDevice()
	{
			SharedContext = new FPlatformOpenGLContext;

			WindowHandle = "#canvas";
	#ifdef __EMSCRIPTEN_PTHREADS__
			thread = 0;
	#endif
			PlatformCreateOpenGLContext(this, (void*)WindowHandle);
	}

	FPlatformOpenGLContext* SharedContext = nullptr;

	const char* WindowHandle;
#ifdef __EMSCRIPTEN_PTHREADS__
	pthread_t thread;
#endif
};

FPlatformOpenGLDevice* PlatformCreateOpenGLDevice()
{
	return new FPlatformOpenGLDevice;
}

bool PlatformCanEnableGPUCapture()
{
	return false;
}

void PlatformReleaseOpenGLContext(FPlatformOpenGLDevice* Device, FPlatformOpenGLContext* Context)
{
#if 0
	check(Device);
	FOpenGL::DeleteFramebuffers(1, &Device->SharedContext->ViewportFramebuffer);
	Device->SharedContext->ViewportFramebuffer = 0;
	wonder_log::info("destroying context");
	SEND(emscripten_webgl_make_context_current, 0);
	SEND(emscripten_webgl_destroy_context, Device->SharedContext->Context);
#ifdef __EMSCRIPTEN_PTHREADS__
	if (Device->thread)
	{
		pthread_exit(0);
		Device->thread = 0;
	}
#endif
	Device->SharedContext->Context = 0;
#endif
}

void PlatformDestroyOpenGLDevice(FPlatformOpenGLDevice* Device)
{
	PlatformReleaseOpenGLContext(Device, NULL);
}

FPlatformOpenGLContext* PlatformGetOpenGLRenderingContext(FPlatformOpenGLDevice* Device)
{
	return Device->SharedContext;
}
APPLICATIONCORE_API void viewportUpdate(int width, int height) {
	//FOpenGL::Viewport(0, 0, width, height);
}
extern OPENGLDRV_API bool canvas_size_changed;
/*
EM_BOOL captureResizeEvent(int eventType, const EmscriptenUiEvent* e, void* rawState) {
	int newWidth = (int)e->windowInnerWidth;
	int newHeight = (int)e->windowInnerHeight;
	EXEC_START
	emscripten_set_canvas_element_size("#canvas", newWidth, newHeight);
	EXEC_END
	FOpenGL::Viewport(0, 0, newWidth, newHeight);
	canvas_size_changed = true;
	return 0;
}*/

//extern "C" __attribute__((used, visibility("default"))) void observeResize(const char* element, void* callback);
size_t startGL() {
	EmscriptenWebGLContextAttributes attr;
	emscripten_webgl_init_context_attributes(&attr);
	attr.alpha = 0;
	attr.depth = 1;
	attr.stencil = 0;
	attr.antialias = PLATFORM_SIMPLYSTREAM_ENABLE_ANTIALIAS;
	attr.majorVersion = 2; // TODO: Set to 2 if targeting WebGL 2
	attr.enableExtensionsByDefault = 1;
	attr.proxyContextToMainThread = 0;
	attr.renderViaOffscreenBackBuffer = 0;
	attr.preserveDrawingBuffer = 1;
	attr.explicitSwapControl = 0;
	if (GUseThreadedRendering)
	{
		attr.explicitSwapControl = 1;
		wonder_log::info("Multithreading RHI enabled");
	}
	else
	{
		wonder_log::info("Multithreading RHI disabled");
	}
	auto ctx = emscripten_webgl_create_context("#canvas", &attr);
	EMSCRIPTEN_RESULT r = emscripten_webgl_make_context_current(ctx);
	wonder_log::info("GUseThreadedRendering {}", GUseThreadedRendering);
	return ctx;
}
void _SimplyStreamCreateContext(FPlatformOpenGLDevice* Device, void* InWindowHandle)
{
#if 0
	if (Device->SharedContext->Context) return;

	if (GUseThreadedRendering) {
		SEND_AND_WAIT(startGL);
		Device->SharedContext->Context = 1;
	}
	else {
		Device->SharedContext->Context = startGL();
	}
	FOpenGL::GenVertexArrays(1, &Device->SharedContext->VertexArrayObject);
	FOpenGL::BindVertexArray(Device->SharedContext->VertexArrayObject);
	canvas_size_changed = true;
#endif
}

FPlatformOpenGLContext* PlatformCreateOpenGLContext(FPlatformOpenGLDevice* Device, void* InWindowHandle)
{
	_SimplyStreamCreateContext(Device, InWindowHandle);
	return Device->SharedContext;
}

void PlatformDestroyOpenGLContext(FPlatformOpenGLDevice* Device, FPlatformOpenGLContext* Context)
{
	PlatformReleaseOpenGLContext(Device, Context);
}

void* PlatformGetWindow(FPlatformOpenGLContext* Context, void** AddParam)
{
	check(Context);

	return (void*)Context->Context;
}
#if 0
std::vector<Frame> frames(1000);
#endif
uint32_t frameIndex = 0;
std::pair<uint64_t, uint64_t> compareSame(int frameA, int frameB) {
	uint64_t same = 0;
	uint64_t not_same = 0;
#if 0
	auto& f1 = frames[frameA];
	auto& f2 = frames[frameB];
	for (auto& u1 : f1.values1) {
		for (auto& u2 : f2.values1) {
			if (u1 == u2) {
				same++;
			}
			else {
				not_same++;
			}
		}
	}
	for (auto& u1 : f1.uniforms1) {
		for (auto& u2 : f2.uniforms1) {
			if (u1 == u2) {
				same++;
			}
			else {
				not_same++;
			}
		}
	}
#endif
	return {same, not_same};
}
std::pair<uint64_t, uint64_t> compareSameCrossFrames() {
	uint64_t same = 0;
	uint64_t not_same = 0;
#if 0
	for (int f1 = 0; f1 < frames.size(); f1++) {
		for (int f2 = 0; f2 < frames.size(); f2++) {
			auto [s, ns] = compareSame(f1, f2);
			same += s;
			not_same += ns;
		}
	}
#endif
	return { same, not_same };
}
std::pair<size_t, size_t> countUniqueAndNonUnique()
{
#if 0
	// Create an unordered set to store the values that have been seen.
	// This will allow us to quickly check if a value has already been seen
	// and count the number of unique values.
	std::unordered_set<uint64_t> seen;

	// Initialize the counters for the number of unique and non-unique values.
	size_t uniqueCount = 0;
	size_t nonUniqueCount = 0;

	// Iterate over each vector of uint64_t values.
	int i = 0;
	for (const auto& frame : frames)
	{
		if (i++ > frameIndex) {
			break;
		}
		// Iterate over each value in the vector.
		for (const auto& value : frame.hashes)
		{
			// If the value has not been seen before, increment the unique value counter
			// and add the value to the set of seen values.
			if (seen.count(value) == 0)
			{
				uniqueCount++;
				seen.insert(value);
			}
			// Otherwise, increment the non-unique value counter.
			else
			{
				nonUniqueCount++;
			}
		}
	}

	// Return the counters for the number of unique and non-unique values.
	return { uniqueCount, nonUniqueCount };
}
KEEP_IN_MODULE void printFrames() {
	EM_ASM(console.log('printFrames', $0), frameIndex);
	auto counts = countUniqueAndNonUnique();
#if 0
	for (int i = 0; i < frames.size(); i++) {
		std::string str = fmt::format("void frame{}() {{ //glUniform4f count {} glUniform4fv count {}\n", i, frames[i].uniforms1.size(), frames[i].uniforms2.size());
		for (int j = 0; j < frames[i].values2.size(); j++) {
			str += frames[i].values2[j] + "\n";
		}
		str += fmt::format("START_TIMER\n");
		for (int j = 0; j < frames[i].uniforms1.size(); j++) {
			str += frames[i].uniforms1[j] + "\n";
		}
		for (int j = 0; j < frames[i].uniforms2.size(); j++) {
			str += frames[i].uniforms2[j] + "\n";
		}
		str += fmt::format("END_TIMER\n");
		str += "}\n";
		auto [s, ns] = compareSame(i, i);
		str += fmt::format("//frame same: {} notsame: {} total: {}\n", s, ns, s + ns);
		EM_ASM(console.log(UTF8ToString($0)), str.data());
	}
#endif
	auto [s, ns] = compareSameCrossFrames();
	//std::string str = fmt::format("//unique: {} non-unique: {} total: {}\n", counts.first, counts.second, counts.first + counts.second);
	//EM_ASM(console.log(UTF8ToString($0)), str.data());
	exit(1);
#endif
	return {};
}
extern uint32_t skip_global_f;
extern uint32_t skip_global_fv;
extern uint32_t skip_ub_f;
extern uint32_t skip_ub_fv;
extern uint32_t do_global_f;
extern uint32_t do_global_fv;
extern uint32_t do_ub_f;
extern uint32_t do_ub_fv;
bool PlatformBlitToViewport(FPlatformOpenGLDevice* Device, const FOpenGLViewport& Viewport, uint32 BackbufferSizeX, uint32 BackbufferSizeY, bool bPresent, bool bLockToVsync) {
#if 0
	const double CurrentTime = FApp::GetCurrentTime();
	const double DeltaTime = CurrentTime - FApp::GetLastTime();
	const double RawFrameTime = DeltaTime * 1000.0;
	if (frameIndex < 1000) {
		frameIndex++;
	}
	else {
		//printFrames();
	}
#ifdef __EMSCRIPTEN_PTHREADS__
	if (GUseThreadedRendering)
	{
		//SEND_NOARGS_FLUSH(emscripten_webgl_commit_frame);
		SEND_AND_WAIT_NOARGS_FLUSH(emscripten_webgl_commit_frame);
	}
#if !USING_FUNCTION && !USE_NETWORK_LOCAL
	SEND_NOARGS(flushGL);
	extern int8_t SendRCSData(bool wait);
	SendRCSData(false);
	//exec.Wait();
#endif
#endif
#endif
	return true;
}

void PlatformRenderingContextSetup(FPlatformOpenGLDevice* Device)
{
	return;
}

void PlatformFlushIfNeeded()
{
#if 0
	//wonder_log::info("PlatformFlushIfNeeded");
	SEND_AND_WAIT_NOARGS_FLUSH(glFlush);
	//SEND_AND_WAIT_NOARGS_FLUSH(glFinish);
#endif
}

void PlatformRebindResources(FPlatformOpenGLDevice* Device)
{
}

void PlatformSharedContextSetup(FPlatformOpenGLDevice* Device)
{
	PlatformRenderingContextSetup(Device);
}

void PlatformNULLContextSetup()
{
#if 0
	return;
	SEND(emscripten_webgl_make_context_current, 0);
#endif
}

EOpenGLCurrentContext PlatformOpenGLCurrentContext(FPlatformOpenGLDevice* Device)
{
	check(PlatformContextIsCurrent((uint64)Device->SharedContext->Context));
	return CONTEXT_Shared;
}

void* PlatformOpenGLCurrentContextHandle(FPlatformOpenGLDevice* Device)
{
	return(void*)Device->SharedContext->Context;
}

void PlatformResizeGLContext(FPlatformOpenGLDevice* Device, FPlatformOpenGLContext* Context, uint32 SizeX, uint32 SizeY, bool bFullscreen, bool bWasFullscreen, GLenum BackBufferTarget, GLuint BackBufferResource)
{
	check(Context);
	VERIFY_GL_SCOPE();

	UE_LOG(LogSimplyStreamOpenGL, Warning, TEXT("PlatformResizeGLContext(%d,%d)"), SizeX, SizeY);

	emscripten_set_canvas_element_size(Device->WindowHandle, SizeX, SizeY);

	//FOpenGL::Viewport(0, 0, SizeX, SizeY);
}

std::pair<uint32, uint32> get_canvas_size()
{
	uint32 Width;
	uint32 Height;
	emscripten_get_canvas_element_size("#canvas", (int*)&Width, (int*)&Height);
	wonder_log::info("get_canvas_size {} {}", Width, Height);
	return { Width, Height };
}

void PlatformGetSupportedResolution(uint32& Width, uint32& Height)
{
#if 0
	std::pair<uint32, uint32> wh;
	SEND_AND_WAIT_RET_NOARGS(get_canvas_size, wh);
	Width = wh.first;
	Height = wh.second;
	wonder_log::info("PlatformGetSupportedResolution {} {}", Width, Height);
#endif
}

bool PlatformGetAvailableResolutions(FScreenResolutionArray& Resolutions, bool bIgnoreRefreshRate)
{
	return true;
}

bool PlatformInitOpenGL()
{
	UE_LOG(LogTemp, Warning, TEXT("PlatformInitOpenGL"));
	return true;
}

bool PlatformOpenGLContextValid()
{
	bool b = true;
#if 0
	EXEC_START_WAIT
		b = emscripten_webgl_get_current_context() != 0;
	EXEC_END
#endif
	return b;
}

int32 PlatformGlGetError()
{
#if 0
	return FOpenGL::GetError();
#endif
	return 0;
}

void PlatformGetBackbufferDimensions(uint32& OutWidth, uint32& OutHeight)
{
	PlatformGetSupportedResolution(OutWidth, OutHeight);
	//UE_LOG(LogSimplyStreamOpenGL, Warning, TEXT("PlatformGetBackbufferDimensions(%d, %d)"), OutWidth, OutHeight);
}

// =============================================================

bool PlatformContextIsCurrent(uint64 QueryContext)
{
	bool b = true;
#if 0
	EXEC_START_WAIT
		b = emscripten_webgl_get_current_context() == (EMSCRIPTEN_WEBGL_CONTEXT_HANDLE)QueryContext;
	EXEC_END
#endif
	return b;
}

FOpenGLTexture* PlatformCreateBuiltinBackBuffer(FOpenGLDynamicRHI* OpenGLRHI, uint32 SizeX, uint32 SizeY)
{
	//EM_ASM(console.error('PlatformCreateBuiltinBackBuffer', $0, $1), SizeX, SizeY);
	UE_LOG(LogSimplyStreamOpenGL, Verbose, TEXT("PlatformCreateBuiltinBackBuffer(%d, %d)"), SizeX, SizeY);
	const FRHITextureCreateDesc Desc =
		FRHITextureCreateDesc::Create2D(TEXT("PlatformCreateBuiltinBackBuffer"), SizeX, SizeY, PF_B8G8R8A8)
		.SetClearValue(FClearValueBinding::Black)
		.SetFlags(ETextureCreateFlags::RenderTargetable | ETextureCreateFlags::Presentable | ETextureCreateFlags::ResolveTargetable)
		.DetermineInititialState();

	return new FOpenGLTexture(Desc);
}

void PlatformGetNewRenderQuery(GLuint* OutQuery, uint64* OutQueryContext)
{
}

void PlatformReleaseRenderQuery(GLuint Query, uint64 QueryContext)
{
}

void PlatformRestoreDesktopDisplayMode()
{
	MAIN_THREAD_EM_ASM(Module['canvas'].UE_canvas.bIsFullScreen = 0; );
}

#include "UnrealEngine.h" // GSystemResolution
extern "C"
{
	int GSystemResolution_ResX()
	{
		return GSystemResolution.ResX;
	}

	int GSystemResolution_ResY()
	{
		return GSystemResolution.ResY;
	}
}
