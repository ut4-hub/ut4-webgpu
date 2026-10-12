// Copyright Epic Games, Inc. All Rights Reserved.

#include "WebXRHMD.h"
#include "DefaultSpectatorScreenController.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "RHI.h"
#include "RenderGraphBuilder.h"
#include "SceneView.h"
#include "UnrealClient.h"
#include "HAL/IConsoleManager.h"

// Mono fallback for on-device debugging without a recook: -dpcvars=xr.WebXR.MonoOnly=1 keeps the
// session head-tracked but renders mono (Phase 1 behavior).
static TAutoConsoleVariable<int32> CVarWebXRMonoOnly(
	TEXT("xr.WebXR.MonoOnly"),
	0,
	TEXT("WebXR: keep immersive sessions head-tracked but render mono (no stereo views)."),
	ECVF_RenderThreadSafe);

namespace WebXRConversion
{
	// WebXR uses the OpenXR convention: right-handed, Y-up, -Z forward, meters.
	// UE: left-handed, Z-up, +X forward, centimeters. Same mapping as OpenXRCore's ToFVector/ToFQuat.
	static FORCEINLINE FVector ToUEPosition(const float P[3], float WorldToMeters)
	{
		return FVector(-P[2], P[0], P[1]) * WorldToMeters;
	}

	static FORCEINLINE FQuat ToUEQuat(const float Q[4])
	{
		FQuat Result(-Q[2], Q[0], Q[1], -Q[3]);
		Result.Normalize();
		return Result;
	}

	// Decompose a WebXR (GL-convention, column-major) projection matrix into tan-half-angles and
	// rebuild the UE reversed-Z infinite-far matrix (same construction as
	// FFakeStereoRenderingDevice::GetStereoProjectionMatrix). Returns false if the input is degenerate.
	static bool ToUEProjection(const float M[16], float ZNear, FMatrix& Out)
	{
		const float A = M[0];   // m00
		const float B = M[5];   // m11
		const float C = M[8];   // m20 (column-major: column 2, row 0)
		const float D = M[9];   // m21
		if (FMath::Abs(A) < KINDA_SMALL_NUMBER || FMath::Abs(B) < KINDA_SMALL_NUMBER)
		{
			return false;
		}
		const float TanRight = (C + 1.0f) / A;
		const float TanLeft  = (C - 1.0f) / A;
		const float TanUp    = (D + 1.0f) / B;
		const float TanDown  = (D - 1.0f) / B;

		const float SumRL = TanRight + TanLeft;
		const float SumTB = TanUp + TanDown;
		const float InvRL = 1.0f / (TanRight - TanLeft);
		const float InvTB = 1.0f / (TanUp - TanDown);

		Out = FMatrix(
			FPlane(2.0f * InvRL, 0.0f, 0.0f, 0.0f),
			FPlane(0.0f, 2.0f * InvTB, 0.0f, 0.0f),
			FPlane(SumRL * -InvRL, SumTB * -InvTB, 0.0f, 1.0f),
			FPlane(0.0f, 0.0f, ZNear, 0.0f));
		return true;
	}
}

FWebXRHMD::FWebXRHMD()
	: FHeadMountedDisplayBase(nullptr)
{
	// Default-mode spectator/mirror support; harmless if the platform never routes it.
	SpectatorScreenController = MakeUnique<FDefaultSpectatorScreenController>(this);
	GameState.EyeProjection[0] = GameState.EyeProjection[1] = FMatrix::Identity;
	GameState.EyeTransform[0] = GameState.EyeTransform[1] = FTransform::Identity;
}

void FWebXRHMD::ConvertSnapshot(const FWebXRPoseSnapshot& Raw, FConvertedXRState& Out) const
{
	const float WorldToMeters = CachedWorldToMeters;
	Out.SessionState = Raw.SessionState;
	Out.bPoseValid = (Raw.Flags & 1u) != 0;
	Out.RecommendedEyeWidth = Raw.RecommendedEyeWidth;
	Out.RecommendedEyeHeight = Raw.RecommendedEyeHeight;

	if (Out.bPoseValid)
	{
		const FQuat HeadQ = WebXRConversion::ToUEQuat(Raw.HeadQuat);
		const FVector HeadP = WebXRConversion::ToUEPosition(Raw.HeadPos, WorldToMeters);
		// Apply the recenter base transform.
		Out.HeadOrientation = BaseOrientationInv * HeadQ;
		Out.HeadPosition = BaseOrientationInv.RotateVector(HeadP - BasePosition);
		for (int32 Eye = 0; Eye < 2; ++Eye)
		{
			const FQuat EyeQ = BaseOrientationInv * WebXRConversion::ToUEQuat(Raw.Eye[Eye].Quat);
			const FVector EyeP = BaseOrientationInv.RotateVector(
				WebXRConversion::ToUEPosition(Raw.Eye[Eye].Pos, WorldToMeters) - BasePosition);
			Out.EyeTransform[Eye] = FTransform(EyeQ, EyeP);
		}
	}

	Out.bProjectionValid = true;
	for (int32 Eye = 0; Eye < 2; ++Eye)
	{
		if (!WebXRConversion::ToUEProjection(Raw.Eye[Eye].Proj, GNearClippingPlane, Out.EyeProjection[Eye]))
		{
			Out.bProjectionValid = false;
		}
	}
}

bool FWebXRHMD::EnumerateTrackedDevices(TArray<int32>& OutDevices, EXRTrackedDeviceType Type)
{
	if (Type == EXRTrackedDeviceType::Any || Type == EXRTrackedDeviceType::HeadMountedDisplay)
	{
		OutDevices.Add(IXRTrackingSystem::HMDDeviceId);
		return true;
	}
	return false;
}

bool FWebXRHMD::GetCurrentPose(int32 DeviceId, FQuat& OutOrientation, FVector& OutPosition)
{
	if (DeviceId != IXRTrackingSystem::HMDDeviceId)
	{
		return false;
	}

	if (IsInRenderingThread())
	{
		// Late update: re-read the freshest JS snapshot (FDefaultXRCamera consumes this).
		FWebXRPoseSnapshot Raw;
		if (WebXRSnapshot::TryReadPose(Raw) && (Raw.Flags & 1u) != 0)
		{
			FConvertedXRState Fresh;
			ConvertSnapshot(Raw, Fresh);
			OutOrientation = Fresh.HeadOrientation;
			OutPosition = Fresh.HeadPosition;
			return true;
		}
	}

	if (!GameState.bPoseValid)
	{
		OutOrientation = FQuat::Identity;
		OutPosition = FVector::ZeroVector;
		return false;
	}
	OutOrientation = GameState.HeadOrientation;
	OutPosition = GameState.HeadPosition;
	return true;
}

bool FWebXRHMD::GetRelativeEyePose(int32 DeviceId, int32 ViewIndex, FQuat& OutOrientation, FVector& OutPosition)
{
	if (DeviceId == IXRTrackingSystem::HMDDeviceId && ViewIndex < 2 && GameState.bPoseValid)
	{
		const FTransform HeadTransform(GameState.HeadOrientation, GameState.HeadPosition);
		const FTransform RelEye = GameState.EyeTransform[ViewIndex].GetRelativeTransform(HeadTransform);
		OutOrientation = RelEye.GetRotation();
		OutPosition = RelEye.GetTranslation();
		return true;
	}
	// Fall back to the base implementation (default IPD offsets) when no live eye data exists.
	return FXRTrackingSystemBase::GetRelativeEyePose(DeviceId, ViewIndex, OutOrientation, OutPosition);
}

void FWebXRHMD::ResetOrientationAndPosition(float Yaw)
{
	FWebXRPoseSnapshot Raw;
	if (WebXRSnapshot::TryReadPose(Raw) && (Raw.Flags & 1u) != 0)
	{
		const FQuat HeadQ = WebXRConversion::ToUEQuat(Raw.HeadQuat);
		const FVector HeadP = WebXRConversion::ToUEPosition(Raw.HeadPos, CachedWorldToMeters);
		// Yaw-only recenter: keep the horizon level.
		const FRotator YawOnly(0.0f, FRotator(HeadQ).Yaw + Yaw, 0.0f);
		BaseOrientationInv = YawOnly.Quaternion().Inverse();
		BasePosition = HeadP;
	}
}

bool FWebXRHMD::OnStartGameFrame(FWorldContext& WorldContext)
{
	if (UWorld* World = WorldContext.World())
	{
		if (AWorldSettings* WorldSettings = World->GetWorldSettings())
		{
			CachedWorldToMeters = WorldSettings->WorldToMeters;
		}
	}

	FWebXRPoseSnapshot Raw;
	if (WebXRSnapshot::TryReadPose(Raw))
	{
		ConvertSnapshot(Raw, GameState);
	}

	// Session state machine: 2=Visible 3=VisibleBlurred 4=Hidden are "in session".
	const bool bNowActive = GameState.SessionState >= 2 && GameState.SessionState <= 4;
	if (bNowActive != bSessionActive)
	{
		bSessionActive = bNowActive;
		if (bSessionActive)
		{
			UE_LOG(LogHMD, Log, TEXT("WebXR session active (state=%u, recommended eye %ux%u)"),
				GameState.SessionState, GameState.RecommendedEyeWidth, GameState.RecommendedEyeHeight);
			SetHeadTrackingEnforced(true);
			if (CVarWebXRMonoOnly.GetValueOnGameThread() == 0)
			{
				EnableStereo(true);
			}
		}
		else
		{
			UE_LOG(LogHMD, Log, TEXT("WebXR session ended (state=%u)"), GameState.SessionState);
			EnableStereo(false);
			SetHeadTrackingEnforced(false);
		}
	}
	return true;
}

void FWebXRHMD::OnBeginRendering_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& ViewFamily)
{
	FXRTrackingSystemBase::OnBeginRendering_RenderThread(GraphBuilder, ViewFamily);

	// Publish the texture JS should copy into the XR layer: the family's render target — the
	// engine-allocated side-by-side target in stereo, the viewport backbuffer in mono.
	if (const FRenderTarget* RenderTarget = ViewFamily.RenderTarget)
	{
		if (FRHITexture* Texture = RenderTarget->GetRenderTargetTexture())
		{
			const FIntPoint Size = Texture->GetSizeXY();
			WebXRSnapshot::PublishFrameInfo(
				reinterpret_cast<uint64>(Texture->GetNativeResource()),
				static_cast<uint32>(Size.X),
				static_cast<uint32>(Size.Y),
				static_cast<uint32>(Texture->GetFormat()), // EPixelFormat; JS reads the real format off the GPUTexture
				IsStereoEnabled() ? 1u : 0u);
		}
	}
}

void FWebXRHMD::GetFieldOfView(float& InOutHFOVInDegrees, float& InOutVFOVInDegrees) const
{
	// Derive from the live left-eye projection when available.
	if (GameState.bProjectionValid)
	{
		const FMatrix& P = GameState.EyeProjection[0];
		if (P.M[0][0] > KINDA_SMALL_NUMBER && P.M[1][1] > KINDA_SMALL_NUMBER)
		{
			InOutHFOVInDegrees = 2.0f * FMath::RadiansToDegrees(FMath::Atan(1.0f / P.M[0][0]));
			InOutVFOVInDegrees = 2.0f * FMath::RadiansToDegrees(FMath::Atan(1.0f / P.M[1][1]));
			return;
		}
	}
	InOutHFOVInDegrees = 90.0f;
	InOutVFOVInDegrees = 90.0f;
}

float FWebXRHMD::GetInterpupillaryDistance() const
{
	if (GameState.bPoseValid)
	{
		const float DistUU = static_cast<float>(
			FVector::Dist(GameState.EyeTransform[0].GetTranslation(), GameState.EyeTransform[1].GetTranslation()));
		if (DistUU > KINDA_SMALL_NUMBER)
		{
			return DistUU / CachedWorldToMeters; // meters
		}
	}
	return 0.064f;
}

FIntPoint FWebXRHMD::GetIdealRenderTargetSize() const
{
	if (GameState.RecommendedEyeWidth > 0 && GameState.RecommendedEyeHeight > 0)
	{
		return FIntPoint(static_cast<int32>(GameState.RecommendedEyeWidth) * 2,
		                 static_cast<int32>(GameState.RecommendedEyeHeight));
	}
	return FIntPoint(2048, 1024);
}

bool FWebXRHMD::EnableStereo(bool bEnable)
{
	bStereoEnabled = bEnable;
	return bStereoEnabled;
}

void FWebXRHMD::AdjustViewRect(const int32 ViewIndex, int32& X, int32& Y, uint32& SizeX, uint32& SizeY) const
{
	SizeX = SizeX / 2;
	X += SizeX * ViewIndex;
}

FMatrix FWebXRHMD::GetStereoProjectionMatrix(const int32 ViewIndex) const
{
	if (GameState.bProjectionValid && ViewIndex >= 0 && ViewIndex < 2)
	{
		return GameState.EyeProjection[ViewIndex];
	}
	// Symmetric 90° fallback (reversed-Z, infinite far) until the first XR frame arrives.
	const float HalfFov = FMath::DegreesToRadians(90.0f) / 2.0f;
	const float XS = 1.0f / FMath::Tan(HalfFov);
	const float YS = 1.0f / FMath::Tan(HalfFov);
	return FMatrix(
		FPlane(XS, 0.0f, 0.0f, 0.0f),
		FPlane(0.0f, YS, 0.0f, 0.0f),
		FPlane(0.0f, 0.0f, 0.0f, 1.0f),
		FPlane(0.0f, 0.0f, GNearClippingPlane, 0.0f));
}
