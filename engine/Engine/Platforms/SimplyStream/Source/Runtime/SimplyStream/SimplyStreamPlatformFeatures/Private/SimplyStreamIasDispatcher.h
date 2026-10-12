// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

// wndr IAS (Engine/Platforms/SimplyStream/Upgrade/iostore_and_ranges_plan.md — M2): plugin-side HTTP I/O
// dispatcher install. M2.1 installs a skeleton dispatcher as early as possible (first-writer-wins; zero
// engine edit) and logs whether we won ownership of GHttpIoDispatcher — validating the install-ordering risk.
namespace UE::SimplyStream
{
	void InstallSimplyStreamIasDispatcher();
}
