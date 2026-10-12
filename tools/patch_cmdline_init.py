#!/usr/bin/env python3
"""Fix: FCommandLine not initialized when pre-init probes read the command line.

The wndr standalone path (main2) writes the GCmdLine global directly and skips
SimplyStreamMain() (which normally reaches LaunchEngineLoop::PreInit ->
FCommandLine::Set). The AR-fix probe added 2026-10-10 in SIMPLYSTREAM_Init calls
IFileManager::Get().FileExists(...) BEFORE PreInitPreStartupScreen; that reaches
PathPak_FileExists -> EnsurePathPakMounted -> FCommandLine::Get() while
bIsInitialized == false -> UE_LOG Fatal:
  "Attempting to get the command line but it hasn't been initialized yet."
Fix: publish the command line via FCommandLine::Set(GCmdLine) in main2 right
after building it, so the whole pre-init window sees a valid command line.
"""
import sys

P = "/mnt/vms/ss-build/UnrealEngine/Engine/Platforms/SimplyStream/Source/Runtime/Launch/Private/LaunchSimplyStream.cpp"

s = open(P).read()

old_inc = '#include "Misc/ConfigCacheIni.h"\n'
new_inc = '#include "Misc/ConfigCacheIni.h"\n#include "Misc/CommandLine.h"\n'
assert s.count(old_inc) == 1, "include anchor not unique/found"
s = s.replace(old_inc, new_inc)

old = '\tFCString::Strcat(GCmdLine, ANSI_TO_TCHAR(" -stdout"));\n'
new = old + '''
	// wndr (2026-10-10): initialize FCommandLine BEFORE any early FileExists() probe.
	// The standalone path writes the GCmdLine global directly and skips SimplyStreamMain()
	// (which normally reaches LaunchEngineLoop::PreInit -> FCommandLine::Set). The AR-fix
	// probe in SIMPLYSTREAM_Init can reach PathPak FileExists -> EnsurePathPakMounted ->
	// FCommandLine::Get() while bIsInitialized==false -> Fatal "Attempting to get the
	// command line but it hasn't been initialized yet.". Publish it here so the whole
	// pre-init window sees a valid command line.
	FCommandLine::Set(GCmdLine);
'''
assert s.count(old) == 1, "main2 anchor not unique/found"
s = s.replace(old, new)

open(P, "w").write(s)
print("PATCH OK:", P)
