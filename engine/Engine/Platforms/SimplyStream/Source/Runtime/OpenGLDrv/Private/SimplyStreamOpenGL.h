// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SimplyStreamOpenGL.h: Public OpenGL ES 2.0 definitions for SimplyStream-specific functionality
=============================================================================*/

#pragma once

#define GL_GLEXT_PROTOTYPES

//#include <EGL/eglext.h>
//#include <EGL/eglplatform.h>
#include <GLES3/gl3.h>
#include <GLES2/gl2ext.h>

typedef GLsync			UGLsync;
#define GLdouble		GLfloat
typedef khronos_int64_t GLint64;
typedef khronos_uint64_t GLuint64;
#define GL_CLAMP		GL_CLAMP_TO_EDGE

#include "OpenGLES.h"
#include "SimplyStreamRHIShaderPlatform.h"

#ifndef GL_LUMINANCE8_EXT
#define GL_LUMINANCE8_EXT GL_LUMINANCE
#endif

#ifndef GL_BGRA8_EXT
#define GL_BGRA8_EXT GL_BGRA
#endif

#ifndef GL_ALPHA8_EXT
#define GL_ALPHA8_EXT GL_ALPHA
#endif


#ifndef GL_TEXTURE_1D
#define GL_TEXTURE_1D			0x0DE0
#endif

#ifndef GL_TEXTURE_1D_ARRAY
#define GL_TEXTURE_1D_ARRAY		0x8C18
#endif

#ifndef GL_TEXTURE_2D_ARRAY
#define GL_TEXTURE_2D_ARRAY		0x8C1A
#endif

#ifndef GL_TEXTURE_RECTANGLE
#define GL_TEXTURE_RECTANGLE	0x84F5
#endif

/** Unreal tokens that maps to different OpenGL tokens by platform. */
#ifndef GL_DRAW_FRAMEBUFFER
#define GL_DRAW_FRAMEBUFFER	GL_FRAMEBUFFER
#endif
#ifndef GL_READ_FRAMEBUFFER
#define GL_READ_FRAMEBUFFER GL_FRAMEBUFFER
#endif




// dont do any UE4 OpenGLDebug stuff
#ifdef GL_ARB_debug_output
#undef GL_ARB_debug_output
#endif
#ifdef GL_KHR_debug
#undef GL_KHR_debug
#endif

struct FSimplyStreamOpenGL : public FOpenGLES {
	static FORCEINLINE bool IsSync(UGLsync Sync) { return true;	}

	static FORCEINLINE EFenceResult ClientWaitSync(UGLsync Sync, GLbitfield Flags, GLuint64 Timeout) {
		return FR_ConditionSatisfied;
	}

	static FORCEINLINE bool SupportsImageExternal() { return true; }
	static FORCEINLINE bool SupportsFastBufferData() {
		return true;
	}

	static FORCEINLINE bool DiscardFrameBufferToResize() {
		// In browsers calling glBufferData() to discard-reupload is slower than calling glBufferSubData(),
		// because changing glBufferData() with a different size from before incurs security related validation.
		// Therefore never use the glBufferData() discard trick on SimplyStream builds.
		return false;
	}

	static FORCEINLINE void	CopyTexSubImage1D(GLenum Target, GLint Level, GLint XOffset, GLint X, GLint Y, GLsizei Width)
	{
		checkNoEntry();
	}

	static FORCEINLINE bool SupportsMapBuffer()							{ return true; } //webgl2 does not have map buffer by default, so check for support with extensions
	static FORCEINLINE bool SupportsCombinedDepthStencilAttachment()	{ return true; }
	static FORCEINLINE bool SupportsMultipleRenderTargets()				{ return false; }
	static FORCEINLINE bool SupportsInstancing()						{ return true; }
	static FORCEINLINE bool SupportsDrawBuffers()						{ return true; }
	static FORCEINLINE bool SupportsUniformBuffers()					{ return true; }
	static FORCEINLINE bool SupportsBlitFramebuffer()					{ return true; }
	static FORCEINLINE bool SupportsSRGB() 								{ return true; }
	static FORCEINLINE GLenum GetDepthFormat()							{ return GL_DEPTH_COMPONENT24; }
	static FORCEINLINE GLenum GetShadowDepthFormat()					{ return GL_DEPTH_COMPONENT16; }
	static FORCEINLINE bool SupportsResourceView()						{ return false; }
	// Optional
	static FORCEINLINE void BeginQuery(GLenum QueryType, GLuint QueryId) UGL_OPTIONAL_VOID
	static FORCEINLINE void EndQuery(GLenum QueryType) UGL_OPTIONAL_VOID
	static FORCEINLINE void LabelObject(GLenum Type, GLuint Object, const ANSICHAR* Name)UGL_OPTIONAL_VOID
	static FORCEINLINE void PushGroupMarker(const ANSICHAR* Name)UGL_OPTIONAL_VOID
	static FORCEINLINE void PopGroupMarker() UGL_OPTIONAL_VOID
	static FORCEINLINE void GetQueryObject(GLuint QueryId, EQueryMode QueryMode, uint64 *OutResult) UGL_OPTIONAL_VOID
	static FORCEINLINE void* MapBufferRange(GLenum Type, uint32 InOffset, uint32 InSize, EResourceLockMode LockMode)	UGL_OPTIONAL(NULL)
	static FORCEINLINE void UnmapBuffer(GLenum Type) UGL_OPTIONAL_VOID
	static FORCEINLINE void UnmapBufferRange(GLenum Type, uint32 InOffset, uint32 InSize)UGL_OPTIONAL_VOID
	static FORCEINLINE void GenQueries(GLsizei NumQueries, GLuint* QueryIDs) UGL_OPTIONAL_VOID
	static FORCEINLINE void DeleteQueries(GLsizei NumQueries, const GLuint* QueryIDs )UGL_OPTIONAL_VOID
	static FORCEINLINE void GetQueryObject(GLuint QueryId, EQueryMode QueryMode, GLuint *OutResult)UGL_OPTIONAL_VOID

	static FORCEINLINE void VertexAttribDivisor(GLuint Index, GLuint Divisor) {
		//if (currentVertexAttribDivisor[Index] != Divisor) {
			FOpenGLES::VertexAttribDivisor(Index, Divisor);
		//	currentVertexAttribDivisor[Index] = Divisor;
		//}
	}

	static void ProcessExtensions(const FString& ExtensionsStrings);

	static FORCEINLINE EShaderPlatform GetShaderPlatform() { return SP_OPENGL_ES3_WEBGL; }

	// platform extensions overrides
	static void PE_GetCurrentOpenGLShaderDeviceCapabilities(FOpenGLShaderDeviceCapabilities& Capabilities);
	static bool PE_GLSLToDeviceCompatibleGLSL(FAnsiCharArray& GlslCodeOriginal, const FString& ShaderName, GLenum TypeEnum, const FOpenGLShaderDeviceCapabilities& Capabilities, FAnsiCharArray& GlslCode);
	static void PE_SetupTextureFormat(void(*SetupTextureFormat)(EPixelFormat, const FOpenGLTextureFormat&));

	static FORCEINLINE FString GetAdapterName() { return TEXT(""); }

	static FORCEINLINE GLsizei GetLabelObject(GLenum Type, GLuint Object, GLsizei BufferSize, ANSICHAR* OutName) {
		return 0;
	}

	static FORCEINLINE int32 GetReadHalfFloatPixelsEnum()				{ return GL_HALF_FLOAT; }
	static FORCEINLINE bool SupportsHDR32bppEncodeModeIntrinsic()		{ return true; }
	static FORCEINLINE EPixelFormat PreferredPixelFormatHint(EPixelFormat PreferredPixelFormat) {
		if (PreferredPixelFormat == PF_A2B10G10R10)
		{	// UE-71220: this seems to be FDummyViewport - force to valid pixel format type for SimplyStream
			return PF_FloatRGBA;
		}
		// Use a default pixel format if none was specified
		if (PreferredPixelFormat == EPixelFormat::PF_Unknown)
		{
			return PF_B8G8R8A8;
		}
		return PreferredPixelFormat;
	}

	static FORCEINLINE void Flush() {
		// i.e. DO NOTHING // glFlush();
	}

protected:
	// WebGL API calls are slow, so do a state cache to remember the previous call to glVertexAttribDivisor so that we don't call these if they don't actually change.
	// TODO: When adding support for VAOs, whenever a new VAO is bound, call ResetVertexAttribDivisorCache() to reset this cache.
	static uint8_t currentVertexAttribDivisor[64];

	static void ResetVertexAttribDivisorCache()
	{
		memset(currentVertexAttribDivisor, 0xFF, sizeof(currentVertexAttribDivisor));
	}
};


typedef FSimplyStreamOpenGL FOpenGL;
