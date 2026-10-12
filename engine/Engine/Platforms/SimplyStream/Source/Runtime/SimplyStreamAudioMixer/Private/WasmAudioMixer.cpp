// Copyright Epic Games, Inc. All Rights Reserved.
//
// SimplyStream wasm audio mixer — Web Audio + AudioWorklet backend (C++ side).
//
// Paired with WasmAudioMixerLibrary.js (registered with emcc as a --js-library via
// SimplyStreamAudioMixer.Build.cs). This file implements the engine-facing
// IAudioMixerPlatformInterface; the .js file owns the browser-side AudioContext +
// AudioWorklet and drains the shared ring this class fills.
//
// KEY DESIGN — why there is no hardware callback thread here:
//   The browser gives us no OS audio callback thread we can pump the mixer from.
//   Instead we lean on the engine's NULL-device pump: StartAudioStream() calls
//   StartRunningNullDevice(), whose TimeCritical thread calls ReadNextBuffer() at
//   the buffer cadence, and ReadNextBuffer() calls our SubmitBuffer(). So the
//   null device is the intended *clock/producer*, not a silent fallback. Do NOT
//   call StopRunningNullDevice while playing — that would remove the only thing
//   driving SubmitBuffer and guarantee silence.
//
//   SubmitBuffer() copies the interleaved float samples into a ring buffer that
//   lives in wasm linear memory (a SharedArrayBuffer in this pthread build). The
//   AudioWorklet (JS) reads the exact same bytes via the wasm Memory and plays
//   them out — no per-buffer JS call, no copy across the wasm/JS boundary.
//
// Ring layout at RingMemory (matches WasmAudioMixerLibrary.js exactly):
//   int32 [0] writeIdx  (float-samples, mod bufSamples) — written by C, read by worklet
//   int32 [1] readIdx   (float-samples, mod bufSamples) — written by worklet, read by C
//   int32 [2] bufSamples (capacity in float-samples; const after init)
//   int32 [3] numChannels (interleaved L,R,L,R,…)
//   float [16 bytes …] audio body (bufSamples floats)

#include "CoreMinimal.h"
#include "AudioMixer.h"
#include "AudioMixerLog.h"
#include "AudioMixerTypes.h"
#include "Modules/ModuleManager.h"
#include "HAL/UnrealMemory.h"

#if WITH_ENGINE
#include "AudioMixerDevice.h"
#include "AudioDevice.h"
#endif // WITH_ENGINE

// -----------------------------------------------------------------------------
// Browser bridge — implemented in WasmAudioMixerLibrary.js (emcc --js-library).
// Signatures must match the JS ($WndrAudio) exactly.
//   wndr_audio_init(ringPtr, bufSamples, numChannels) -> sampleRate (0 on failure)
//   wndr_audio_start()            -> 1 on success (resumes ctx + arms gesture hook)
//   wndr_audio_stop()             -> 1 on success (suspends ctx)
//   wndr_audio_get_sample_rate()  -> current AudioContext sample rate
// -----------------------------------------------------------------------------
extern "C"
{
	int wndr_audio_init(float* RingPtr, int BufSamples, int NumChannels);
	int wndr_audio_start();
	int wndr_audio_stop();
	int wndr_audio_get_sample_rate();
}

namespace Audio
{
	namespace WasmAudioRing
	{
		// Ring header: 4 x int32 preceding the float body (see JS: body at ringPtr+16).
		static constexpr int32 HeaderBytes   = 16;
		static constexpr int32 IdxWrite      = 0;
		static constexpr int32 IdxRead       = 1;
		static constexpr int32 IdxBufSamples = 2;
		static constexpr int32 IdxNumChan    = 3;

		// Capacity of the ring in float-samples. Matches the "ring=16384 samples"
		// the worklet reports. ~170ms of stereo @ 48kHz — deep enough to ride out
		// scheduler jitter on the null-device pump thread.
		static constexpr int32 CapacitySamples = 16384;
	}

	class FWasmAudioMixer : public IAudioMixerPlatformInterface
	{
	public:
		FWasmAudioMixer()
			: RingMemory(nullptr)
			, BufSamples(0)
			, NumChannels(2)
			, SampleRate(48000)
			, OutputBufferByteLength(0)
			, bInitialized(false)
			, bSuspended(false)
		{
		}

		virtual ~FWasmAudioMixer()
		{
			if (bInitialized)
			{
				TeardownHardware();
			}
		}

		//~ Begin IAudioMixerPlatformInterface Interface
		virtual FString GetPlatformApi() const override { return TEXT("WebAudio"); }

		virtual bool InitializeHardware() override
		{
			if (bInitialized)
			{
				UE_LOGF(LogAudioMixer, Error, "[WasmAudio] InitializeHardware called twice.");
				return false;
			}

			NumChannels = 2;
			BufSamples  = WasmAudioRing::CapacitySamples;

			// Allocate the ring in wasm linear memory and publish its header. The
			// worklet is handed this same buffer + offset by wndr_audio_init().
			const SIZE_T RingBytes = (SIZE_T)WasmAudioRing::HeaderBytes + (SIZE_T)BufSamples * sizeof(float);
			RingMemory = (uint8*)FMemory::Malloc(RingBytes, 16);
			FMemory::Memzero(RingMemory, RingBytes);

			int32* Header = reinterpret_cast<int32*>(RingMemory);
			Header[WasmAudioRing::IdxWrite]      = 0;
			Header[WasmAudioRing::IdxRead]       = 0;
			Header[WasmAudioRing::IdxBufSamples] = BufSamples;
			Header[WasmAudioRing::IdxNumChan]    = NumChannels;

			// Create the (suspended) AudioContext + load the worklet, sharing the ring.
			const int32 ReportedRate = wndr_audio_init(reinterpret_cast<float*>(RingMemory), BufSamples, NumChannels);
			SampleRate = (ReportedRate > 0) ? ReportedRate : 48000;

			UE_LOGF(LogAudioMixer, Display, "[WasmAudio] initialised @ %d Hz, %ls", SampleRate,
				(NumChannels >= 2) ? TEXT("stereo") : TEXT("mono"));

			if (IAudioMixer::ShouldRecycleThreads())
			{
				// Pre-create the null render device thread paused, so StartAudioStream
				// can wake it cheaply. It does nothing until Resume()'d with our pump.
				CreateNullDeviceThread([] {}, 1.0f, true);
			}

			bInitialized = true;
			return true;
		}

		virtual bool TeardownHardware() override
		{
			if (!bInitialized)
			{
				return true;
			}

			StopAudioStream();
			CloseAudioStream();

			wndr_audio_stop();

			if (RingMemory)
			{
				FMemory::Free(RingMemory);
				RingMemory = nullptr;
			}
			OutputBufferByteLength = 0;
			bInitialized = false;
			return true;
		}

		virtual bool IsInitialized() const override { return bInitialized; }

		virtual bool GetNumOutputDevices(uint32& OutNumOutputDevices) override
		{
			// The browser exposes a single logical output.
			OutNumOutputDevices = 1;
			return true;
		}

		virtual bool GetOutputDeviceInfo(const uint32 /*InDeviceIndex*/, FAudioPlatformDeviceInfo& OutInfo) override
		{
			OutInfo.Reset();
			OutInfo.Name       = TEXT("WebAudio Output");
			OutInfo.DeviceId   = TEXT("WebAudio");
			OutInfo.NumChannels = NumChannels;
			OutInfo.SampleRate  = SampleRate;
			OutInfo.Format      = EAudioMixerStreamDataFormat::Float;
			OutInfo.bIsSystemDefault = true;

			OutInfo.OutputChannelArray.Reset();
			OutInfo.OutputChannelArray.Add(EAudioMixerChannel::FrontLeft);
			if (NumChannels >= 2)
			{
				OutInfo.OutputChannelArray.Add(EAudioMixerChannel::FrontRight);
			}
			return true;
		}

		virtual bool GetDefaultOutputDeviceIndex(uint32& OutDefaultDeviceIndex) const override
		{
			OutDefaultDeviceIndex = AUDIO_MIXER_DEFAULT_DEVICE_INDEX;
			return true;
		}

		virtual bool OpenAudioStream(const FAudioMixerOpenStreamParams& Params) override
		{
			if (!bInitialized || AudioStreamInfo.StreamState != EAudioOutputStreamState::Closed)
			{
				return false;
			}

			OpenStreamParams = Params;

			AudioStreamInfo.Reset();
			AudioStreamInfo.OutputDeviceIndex = OpenStreamParams.OutputDeviceIndex;
			AudioStreamInfo.AudioMixer        = OpenStreamParams.AudioMixer;
			AudioStreamInfo.NumOutputFrames   = OpenStreamParams.NumFrames;
			AudioStreamInfo.NumBuffers        = OpenStreamParams.NumBuffers;

			if (!GetOutputDeviceInfo(AudioStreamInfo.OutputDeviceIndex, AudioStreamInfo.DeviceInfo))
			{
				return false;
			}

			// Bytes the engine will hand us each SubmitBuffer() (float, interleaved).
			OutputBufferByteLength = (int32)OpenStreamParams.NumFrames * AudioStreamInfo.DeviceInfo.NumChannels * (int32)sizeof(float);

			AudioStreamInfo.StreamState = EAudioOutputStreamState::Open;
			return true;
		}

		virtual bool CloseAudioStream() override
		{
			if (AudioStreamInfo.StreamState == EAudioOutputStreamState::Closed)
			{
				return false;
			}

			if (!StopAudioStream())
			{
				return false;
			}

			AudioStreamInfo.StreamState = EAudioOutputStreamState::Closed;
			return true;
		}

		virtual bool StartAudioStream() override
		{
			if (!bInitialized ||
				(AudioStreamInfo.StreamState != EAudioOutputStreamState::Open &&
				 AudioStreamInfo.StreamState != EAudioOutputStreamState::Stopped))
			{
				return false;
			}

			// WNDR ISOLATION TEST: audio playback path disabled to confirm it is the
			// source of the game-thread render stall. If render returns, the pump is the cause.
			//BeginGeneratingAudio();
			//StartRunningNullDevice();
			//wndr_audio_start();

			AudioStreamInfo.StreamState = EAudioOutputStreamState::Running;
			return true;
		}

		virtual bool StopAudioStream() override
		{
			if (!bInitialized)
			{
				return false;
			}

			if (AudioStreamInfo.StreamState != EAudioOutputStreamState::Stopped &&
				AudioStreamInfo.StreamState != EAudioOutputStreamState::Closed)
			{
				wndr_audio_stop();

				if (bIsUsingNullDevice)
				{
					StopRunningNullDevice();
				}

				if (AudioStreamInfo.StreamState == EAudioOutputStreamState::Running)
				{
					StopGeneratingAudio();
				}

				check(AudioStreamInfo.StreamState == EAudioOutputStreamState::Stopped);
			}

			return true;
		}

		virtual FAudioPlatformDeviceInfo GetPlatformDeviceInfo() const override
		{
			return AudioStreamInfo.DeviceInfo;
		}

		// Called on the null-device pump thread via ReadNextBuffer(). Copies the
		// interleaved float block into the shared ring, honouring the worklet's
		// read cursor (drop-on-overrun rather than block, so a suspended context
		// can never deadlock the pump thread).
		virtual void SubmitBuffer(const uint8* Buffer) override
		{
			if (!RingMemory || !Buffer || OutputBufferByteLength <= 0)
			{
				return;
			}

			int32* Header = reinterpret_cast<int32*>(RingMemory);
			float* Ring   = reinterpret_cast<float*>(RingMemory + WasmAudioRing::HeaderBytes);
			const float* Src = reinterpret_cast<const float*>(Buffer);

			const int32 Cap        = BufSamples;
			const int32 NumSamples = OutputBufferByteLength / (int32)sizeof(float);

			// writeIdx is ours; readIdx is produced by the worklet on the audio thread.
			int32 WriteIdx = __atomic_load_n(&Header[WasmAudioRing::IdxWrite], __ATOMIC_RELAXED);
			const int32 ReadIdx = __atomic_load_n(&Header[WasmAudioRing::IdxRead], __ATOMIC_ACQUIRE);

			// Available (unread) samples and free space. Keep one sample of slack so
			// full and empty stay distinguishable (writeIdx never catches readIdx).
			const int32 Avail = (WriteIdx - ReadIdx + Cap) % Cap;
			const int32 Free  = Cap - 1 - Avail;
			const int32 ToWrite = FMath::Min(NumSamples, Free);

			for (int32 i = 0; i < ToWrite; ++i)
			{
				Ring[WriteIdx] = Src[i];
				if (++WriteIdx >= Cap)
				{
					WriteIdx = 0;
				}
			}

			// Publish the new write cursor (release pairs with the worklet's acquire load).
			__atomic_store_n(&Header[WasmAudioRing::IdxWrite], WriteIdx, __ATOMIC_RELEASE);
		}

		virtual FString GetDefaultDeviceName() override
		{
			static const FString DefaultName(TEXT("WebAudio Output"));
			return DefaultName;
		}

		virtual FAudioPlatformSettings GetPlatformSettings() const override
		{
			// Defaults are already 48000Hz / 1024-frame callbacks / 2 buffers.
			FAudioPlatformSettings Settings;
			Settings.SampleRate = (SampleRate > 0) ? SampleRate : 48000;
			return Settings;
		}

		virtual void ResumeContext() override
		{
			if (bSuspended)
			{
				wndr_audio_start();
				bSuspended = false;
			}
		}

		virtual void SuspendContext() override
		{
			if (!bSuspended)
			{
				wndr_audio_stop();
				bSuspended = true;
			}
		}
		//~ End IAudioMixerPlatformInterface Interface

	private:
		// Shared ring: [4 x int32 header][BufSamples x float body], in wasm memory.
		uint8*  RingMemory;
		int32   BufSamples;   // ring capacity in float-samples
		int32   NumChannels;  // 2 (stereo)
		int32   SampleRate;   // AudioContext rate (48000)

		int32   OutputBufferByteLength; // bytes per SubmitBuffer() block

		bool    bInitialized;
		bool    bSuspended;
	};

} // namespace Audio


// -----------------------------------------------------------------------------
// Module registration — mirrors FAudioMixerModuleSDL (AudioMixerSDL.cpp).
// -----------------------------------------------------------------------------
#if WITH_ENGINE

class FSimplyStreamAudioMixerModule : public IAudioDeviceModule
{
public:
	virtual bool IsAudioMixerModule() const override { return true; }

	virtual void StartupModule() override
	{
		IAudioDeviceModule::StartupModule();
		FModuleManager::Get().LoadModuleChecked(TEXT("AudioMixerCore"));
	}

	virtual FAudioDevice* CreateAudioDevice() override
	{
		return new Audio::FMixerDevice(new Audio::FWasmAudioMixer());
	}

	virtual Audio::IAudioMixerPlatformInterface* CreateAudioMixerPlatformInterface() override
	{
		return new Audio::FWasmAudioMixer();
	}
};

IMPLEMENT_MODULE(FSimplyStreamAudioMixerModule, SimplyStreamAudioMixer);

#else // !WITH_ENGINE

// Server / no-engine configurations: nothing to register, but the module must
// still link so the js-library dependency is honoured.
IMPLEMENT_MODULE(FDefaultModuleImpl, SimplyStreamAudioMixer);

#endif // WITH_ENGINE
