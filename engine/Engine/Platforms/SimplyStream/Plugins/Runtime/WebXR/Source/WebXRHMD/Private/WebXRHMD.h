// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "HeadMountedDisplayBase.h"
#include "XRRenderTargetManager.h"
#include "WebXRSnapshot.h"

/**
 * WebXR HMD for the SimplyStream WebGPU/wasm platform (see Upgrade/xr-port.md).
 *
 * The JS main thread owns the XRSession (simplystream.js createWebXRRuntime) and writes head/eye
 * poses + projections into FWebXRPoseSnapshot every XR rAF; this class converts them to UE
 * conventions and drives plain multi-pass stereo. The engine renders into an engine-allocated
 * side-by-side render target; JS copies it into the XR projection layer. No FXRRenderBridge and
 * no custom present are installed (the platform viewport's SetCustomPresent is unimplemented).
 *
 * Game-thread state is refreshed in OnStartGameFrame; GetCurrentPose re-reads the snapshot on the
 * render thread, which is what FDefaultXRCamera consumes for late update.
 */
class FWebXRHMD : public FHeadMountedDisplayBase, public FXRRenderTargetManager, public TSharedFromThis<FWebXRHMD, ESPMode::ThreadSafe>
{
public:
	FWebXRHMD();
	virtual ~FWebXRHMD() = default;

	// IXRSystemIdentifier
	virtual FName GetSystemName() const override { return FName(TEXT("WebXR")); }
	virtual int32 GetXRSystemFlags() const override { return EXRSystemFlags::IsHeadMounted; }

	// IXRTrackingSystem
	virtual FString GetVersionString() const override { return TEXT("WebXR (SimplyStream)"); }
	virtual bool DoesSupportPositionalTracking() const override { return true; }
	virtual bool EnumerateTrackedDevices(TArray<int32>& OutDevices, EXRTrackedDeviceType Type = EXRTrackedDeviceType::Any) override;
	virtual bool GetCurrentPose(int32 DeviceId, FQuat& OutOrientation, FVector& OutPosition) override;
	virtual bool GetRelativeEyePose(int32 DeviceId, int32 ViewIndex, FQuat& OutOrientation, FVector& OutPosition) override;
	virtual void ResetOrientationAndPosition(float Yaw = 0.f) override;
	virtual EHMDTrackingOrigin::Type GetTrackingOrigin() const override { return EHMDTrackingOrigin::LocalFloor; }
	virtual float GetWorldToMetersScale() const override { return CachedWorldToMeters; }
	virtual bool OnStartGameFrame(FWorldContext& WorldContext) override;
	virtual void OnBeginRendering_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& ViewFamily) override;
	virtual IHeadMountedDisplay* GetHMDDevice() override { return this; }
	virtual TSharedPtr<IStereoRendering, ESPMode::ThreadSafe> GetStereoRenderingDevice() override { return SharedThis(this); }

	// IHeadMountedDisplay
	virtual bool IsHMDConnected() override { return true; }
	virtual bool IsHMDEnabled() const override { return bHMDEnabled; }
	virtual void EnableHMD(bool bEnable = true) override { bHMDEnabled = bEnable; }
	virtual bool GetHMDMonitorInfo(MonitorInfo& OutInfo) override { return false; }
	virtual void GetFieldOfView(float& InOutHFOVInDegrees, float& InOutVFOVInDegrees) const override;
	virtual void SetInterpupillaryDistance(float NewInterpupillaryDistance) override {}
	virtual float GetInterpupillaryDistance() const override;
	virtual bool IsChromaAbCorrectionEnabled() const override { return false; }
	virtual FIntPoint GetIdealRenderTargetSize() const override;
	virtual float GetPixelDenity() const override { return 1.0f; }

	// IStereoRendering
	virtual bool IsStereoEnabled() const override { return bStereoEnabled; }
	virtual bool EnableStereo(bool bEnable = true) override;
	virtual void AdjustViewRect(const int32 ViewIndex, int32& X, int32& Y, uint32& SizeX, uint32& SizeY) const override;
	virtual FMatrix GetStereoProjectionMatrix(const int32 ViewIndex) const override;
	virtual IStereoRenderTargetManager* GetRenderTargetManager() override { return this; }

	// IStereoRenderTargetManager (FXRRenderTargetManager supplies CalculateRenderTargetSize /
	// NeedReAllocateViewportRenderTarget from GetIdealRenderTargetSize; the engine allocates the
	// actual side-by-side texture because we do not override AllocateRenderTargetTexture).
	virtual bool ShouldUseSeparateRenderTarget() const override { return IsStereoEnabled(); }

private:
	// One game-thread-converted view of the JS snapshot (UE conventions, cm, left-handed).
	struct FConvertedXRState
	{
		bool bPoseValid = false;
		FQuat HeadOrientation = FQuat::Identity;
		FVector HeadPosition = FVector::ZeroVector;
		FTransform EyeTransform[2];     // tracking space, UE units
		FMatrix EyeProjection[2];       // UE reversed-Z
		bool bProjectionValid = false;
		uint32 RecommendedEyeWidth = 0;
		uint32 RecommendedEyeHeight = 0;
		uint32 SessionState = 0;
	};

	// Converts a raw JS snapshot into UE conventions (applies the recenter base transform).
	void ConvertSnapshot(const FWebXRPoseSnapshot& Raw, FConvertedXRState& Out) const;

	FConvertedXRState GameState;        // game thread copy, refreshed in OnStartGameFrame
	bool bSessionActive = false;
	bool bStereoEnabled = false;
	bool bHMDEnabled = true;
	float CachedWorldToMeters = 100.0f;

	// Recenter offset captured by ResetOrientationAndPosition (tracking space, UE units).
	FQuat BaseOrientationInv = FQuat::Identity;
	FVector BasePosition = FVector::ZeroVector;
};
