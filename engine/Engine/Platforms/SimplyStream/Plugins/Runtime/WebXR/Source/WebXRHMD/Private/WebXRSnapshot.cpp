// Copyright Epic Games, Inc. All Rights Reserved.

#include "WebXRSnapshot.h"

// Same export idiom as LaunchSimplyStream.cpp / WebGPUViewport.cpp (_Pause, _FrameReady): the
// plugin links statically into the monolithic wasm, so these are reachable from JS as
// Module._SIMPLYSTREAM_GetXRPoseBuffer / Module._SIMPLYSTREAM_GetXRFrameInfo. JS feature-detects
// them, so a build without this plugin simply has no XR runtime.
#ifndef KEEP_IN_MODULE
#define KEEP_IN_MODULE extern "C" __attribute__((used, visibility("default")))
#endif

namespace
{
	FWebXRPoseSnapshot GPoseSnapshot = {};
	FWebXRFrameInfo GFrameInfo = {};
}

KEEP_IN_MODULE void* SIMPLYSTREAM_GetXRPoseBuffer()
{
	return &GPoseSnapshot;
}

KEEP_IN_MODULE void* SIMPLYSTREAM_GetXRFrameInfo()
{
	return &GFrameInfo;
}

namespace WebXRSnapshot
{

FWebXRPoseSnapshot* GetPoseBuffer() { return &GPoseSnapshot; }
FWebXRFrameInfo* GetFrameInfo() { return &GFrameInfo; }

bool TryReadPose(FWebXRPoseSnapshot& Out)
{
	for (int32 Attempt = 0; Attempt < 4; ++Attempt)
	{
		const uint32 Seq1 = __atomic_load_n(const_cast<const uint32*>(&GPoseSnapshot.Seq), __ATOMIC_ACQUIRE);
		if (Seq1 & 1u)
		{
			continue; // write in progress
		}
		FMemory::Memcpy(&Out, const_cast<const FWebXRPoseSnapshot*>(&GPoseSnapshot), sizeof(FWebXRPoseSnapshot));
		__atomic_thread_fence(__ATOMIC_ACQUIRE);
		const uint32 Seq2 = __atomic_load_n(const_cast<const uint32*>(&GPoseSnapshot.Seq), __ATOMIC_ACQUIRE);
		if (Seq1 == Seq2)
		{
			return true;
		}
	}
	return false;
}

void PublishFrameInfo(uint64 TexturePtr, uint32 Width, uint32 Height, uint32 WGPUFormat, uint32 StereoMode)
{
	const uint32 Seq = GFrameInfo.Seq;
	__atomic_store_n(const_cast<uint32*>(&GFrameInfo.Seq), Seq | 1u, __ATOMIC_RELEASE);
	__atomic_thread_fence(__ATOMIC_RELEASE);
	GFrameInfo.TexturePtr = TexturePtr;
	GFrameInfo.Width = Width;
	GFrameInfo.Height = Height;
	GFrameInfo.WGPUFormat = WGPUFormat;
	GFrameInfo.StereoMode = StereoMode;
	GFrameInfo.FrameCounter++;
	__atomic_store_n(const_cast<uint32*>(&GFrameInfo.Seq), (Seq | 1u) + 1u, __ATOMIC_RELEASE);
}

} // namespace WebXRSnapshot
