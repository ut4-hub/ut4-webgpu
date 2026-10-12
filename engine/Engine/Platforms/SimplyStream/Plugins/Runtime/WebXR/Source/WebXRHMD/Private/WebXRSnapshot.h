// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

// Shared-memory blocks between the JS main thread (which owns the XRSession) and the engine.
// Each block has exactly ONE writer; the Seq field is a seqlock (odd = write in progress).
// With -pthread the wasm HEAP views are SharedArrayBuffer-backed, so JS Atomics.store on Seq
// pairs with the __ATOMIC_ACQUIRE loads in TryRead below.
//
// !! The byte offsets are mirrored in simplystream.js (createWebXRRuntime). Keep in sync. !!

// Writer: JS XR rAF callback. Readers: game thread (OnStartGameFrame) + render thread (late update).
struct FWebXRPoseSnapshot
{
	volatile uint32 Seq;            // byte   0  seqlock
	uint32 SessionState;            // byte   4  0=None 1=Requested 2=Visible 3=VisibleBlurred 4=Hidden 5=Ending
	uint32 FrameNumber;             // byte   8  XR rAF counter
	uint32 Flags;                   // byte  12  bit0 = viewer pose valid this frame
	float  HeadPos[3];              // byte  16  WebXR reference space, meters, right-handed Y-up
	float  HeadQuat[4];             // byte  28  (x,y,z,w)
	struct FEye
	{
		float Pos[3];               // Eye0 byte 44 / Eye1 byte 136
		float Quat[4];              // Eye0 byte 56 / Eye1 byte 148
		float Proj[16];             // Eye0 byte 72 / Eye1 byte 164  XRView.projectionMatrix, column-major
	} Eye[2];
	uint32 RecommendedEyeWidth;     // byte 228  from the layer subimage viewport
	uint32 RecommendedEyeHeight;    // byte 232
	float  PredictedDisplayTimeMS;  // byte 236
};                                  // total 240 bytes
static_assert(sizeof(FWebXRPoseSnapshot) == 240, "FWebXRPoseSnapshot layout is mirrored in simplystream.js");

// Writer: render thread (FWebXRHMD::OnBeginRendering_RenderThread). Reader: JS main thread.
struct FWebXRFrameInfo
{
	volatile uint32 Seq;            // byte  0  seqlock
	uint32 Pad0;                    // byte  4
	uint64 TexturePtr;              // byte  8  WGPUTexture* of the engine's source texture (FRHITexture::GetNativeResource)
	uint32 Width;                   // byte 16
	uint32 Height;                  // byte 20
	uint32 WGPUFormat;              // byte 24  WGPUTextureFormat enum value of the source texture
	uint32 StereoMode;              // byte 28  0 = mono backbuffer, 1 = side-by-side stereo target
	uint32 FrameCounter;            // byte 32  increments every published frame
	uint32 Pad1;                    // byte 36
};                                  // total 40 bytes
static_assert(sizeof(FWebXRFrameInfo) == 40, "FWebXRFrameInfo layout is mirrored in simplystream.js");

namespace WebXRSnapshot
{
	// The static blocks (JS gets their addresses via the exported SIMPLYSTREAM_GetXRPoseBuffer /
	// SIMPLYSTREAM_GetXRFrameInfo functions in WebXRSnapshot.cpp).
	FWebXRPoseSnapshot* GetPoseBuffer();
	FWebXRFrameInfo* GetFrameInfo();

	// Seqlock read of the JS-written pose block. Returns false if no consistent read was possible
	// (caller keeps its previous copy). Safe from any thread.
	bool TryReadPose(FWebXRPoseSnapshot& Out);

	// Seqlock write of the engine-written frame-info block. Render thread only.
	void PublishFrameInfo(uint64 TexturePtr, uint32 Width, uint32 Height, uint32 WGPUFormat, uint32 StereoMode);
}
