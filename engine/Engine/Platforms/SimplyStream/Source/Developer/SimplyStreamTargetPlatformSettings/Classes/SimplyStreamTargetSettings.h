// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SimplyStreamTargetSettings.h: Declares the USimplyStreamTargetSettings class.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
//#include "Engine/EngineTypes.h"
#include "SimplyStreamTargetSettings.generated.h"

/**
* Enumerates available Profiler Modes
*/
UENUM()
namespace EEmscriptenProfilerMode
{
	enum Type
	{
		None = 0,
		CPU,
		Memory,
		Thread,
	};
}

/**
* Enumerates available LTO modes
*/
UENUM()
namespace EEmscriptenLTOMode
{
	enum Type
	{
		None = 0,
		Thin,
		Full,
	};
}

/**
* Enumerates available render modes
*/
UENUM()
namespace ESimplyStreamRenderMode
{
	enum Type
	{
		WebGL2 = 0,
		WebGPUES31,
		WebGPUSM5,
		WebGPUALL,
	};
}

/**
* Enumerates available Memory modes
*/
UENUM()
namespace EEmscriptenMemoryMode
{
	enum Type
	{
		Memory32 = 0,
		Memory64,
		Memory64_Emulated,
	};
}

/**
 * Implements the settings for the SimplyStream target platform.
 */
UCLASS(config=Engine, defaultconfig)
class SIMPLYSTREAMTARGETPLATFORMSETTINGS_API USimplyStreamTargetSettings : public UObject {
public:
	GENERATED_UCLASS_BODY()

	// ------------------------------------------------------------
	// Use SimplyStream .ini files in the editor
	virtual const TCHAR* GetConfigOverridePlatform() const override	{
		return TEXT("SimplyStream");
	}

	/**
	* Supply ProjectID here, can be found in the developer dashboard.
	*/
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Emscripten, Meta = (DisplayName = "ClientID"))
	FString ClientID;

	/**
	* Supply ProjectID here, can be found in the developer dashboard.
	*/
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Emscripten, Meta = (DisplayName = "ProjectID"))
	FString ProjectID;

	/**
	 * Use Fixed TimeStep
	 */
	UPROPERTY(GlobalConfig, EditAnywhere, Category=Emscripten, Meta = (DisplayName = "Fixed TimeStep (i.e. use requestAnimationFrame)"))
	bool UseFixedTimeStep; // need to make a note of: answerhub 409629

	/**
	 * Use OffscreenCanvas Support (else use Offscreen Framebuffer)
	 * Note: Multithreading rendering option (Experimental)
	 */
	UPROPERTY(GlobalConfig, EditAnywhere, Category=Emscripten, Meta = (DisplayName = "Offscreen Canvas [experimental]"))
	bool OffscreenCanvas;

	/**
	 * Profiler Modes
	 * Show embeded profiler onto the generated page.
	 * NONE   : disable all profiler
	 * CPU    : use this to perform cursory interactive performance profiling
	 * Memory : use this to profile the application usage of the Emscripten HEAP
	 * Thread : use this to profile the application usage of pthreads when targeting multithreaded builds
	 */
	UPROPERTY(GlobalConfig, EditAnywhere, Category=Emscripten)
	TEnumAsByte<EEmscriptenProfilerMode::Type> EmscriptenProfilerMode;

	/**
	 * LTO Modes
	 * Show embeded profiler onto the generated page.
	 * NONE   : disable link time optimizations
	 * Thin   : enable thin LTO - modest increase to build time
	 * Full   : enable full LTO - much slower and potentially more optimized than thin but measure.
	 */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Emscripten, Meta = (DisplayName = "Link Time Optimizations"))
	TEnumAsByte<EEmscriptenLTOMode::Type> EmscriptenLTOMode;

	/**
	 * Memory Modes
	 * Show embeded profiler onto the generated page.
	 * Memory32 : requires least memory usage but limited to 4gb
	 * Memory64 : Experimental - requires more memory but can use more than 4gb, requires browser support
	 * Memory64_emulated : Enable 64 bit support with 4gb limit, mostly used for testing
	 */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Emscripten)
	TEnumAsByte<EEmscriptenMemoryMode::Type> EmscriptenMemoryMode;

	/**
	* Enable profiling via Unreal Insights. Not recommended for production.
	*/
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Emscripten, Meta = (DisplayName = "Enable Unreal Insights"))
	bool EnableUnrealInsights;

	/**
	* Enable Maximum Optimization. Not recommended for development.
	*/
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Emscripten, Meta = (DisplayName = "Enable Maximum Optimizations"))
	bool MaximumOptimizations;

	/** No longer used as we support all by default now
	 * RenderModes
	 * Client	: client side rendering
	 * Hybrid   : simulate on server, render on client
	 * Server   : server side rendering
	 */
	//UPROPERTY(GlobalConfig, EditAnywhere, Category = Emscripten, Meta = (DisplayName = "RenderMode"))
	//TEnumAsByte<ESimplyStreamRenderMode::Type> RenderMode;


	/**
	* Enable WebGL Anti Aliasing.
	*/
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Emscripten, Meta = (DisplayName = "EnableAA"))
	bool EnableAA;

	/**
	 * Enable Memory to grow at runtime. Not recommended for release builds.
	 */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Emscripten, Meta = (DisplayName = "Enable AllocGrowth"))
	bool EnableAllocGrowth;

	UPROPERTY(GlobalConfig, EditAnywhere, Category = Emscripten, Meta = (DisplayName = "Initial Memory in MB"))
	int32 InitialMemoryMB;

	UPROPERTY(GlobalConfig, EditAnywhere, Category = Emscripten, Meta = (DisplayName = "Maximum Memory in MB"))
	int32 MaximumMemoryMB;

	/**
	 * Rendering Modes
	 * WebGL2		  : Legacy ES3.0 forward only rendering
	 * WebGPU Mobile  : ES3.1 rendering
	 * WebGPU Desktop : SM5.0 rendering
	 */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Emscripten, Meta = (DisplayName = "Rendering Mode"))
	TEnumAsByte<ESimplyStreamRenderMode::Type> RenderMode;

	UPROPERTY(GlobalConfig, EditAnywhere, Category = Emscripten, Meta = (DisplayName = "Maximum Memory for SimplyStreamFS in MB, should be less than MaximumMemoryMB"))
	int32 SimplyStreamFSMaximumMB;

	/**
	 * Enable SIMD. Not supported in most browsers yet.
	 */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Emscripten, Meta = (DisplayName = "Enable SIMD"))
	bool EnableSIMD;

	/**
	 * Deploy to routes
	 */
	UPROPERTY(GlobalConfig, EditAnywhere, AdvancedDisplay, Category = Emscripten, Meta = (DisplayName = "Deploy to routes"))
	bool DeployToRoutes;

	UPROPERTY(GlobalConfig, EditAnywhere, AdvancedDisplay, Category = Emscripten, Meta = (DisplayName = "Development Route"))
	FString DevRoute;

	UPROPERTY(GlobalConfig, EditAnywhere, AdvancedDisplay, Category = Emscripten, Meta = (DisplayName = "Test Route"))
	FString TestRoute;

	UPROPERTY(GlobalConfig, EditAnywhere, AdvancedDisplay, Category = Emscripten, Meta = (DisplayName = "Shipping Route"))
	FString ShippingRoute;
};
