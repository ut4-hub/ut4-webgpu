#!/usr/bin/env python3
# EXECBRIDGE-1: Module._wndr_exec(ptr) queues a console command (UTF-8) that the
# local AUTPlayerController runs on its next Tick (game thread). Debug aid.
import sys, shutil, os
P = '/mnt/vms/ss-build/UnrealEngine/UnrealTournament/Source/UnrealTournament/Private/UTPlayerController.cpp'
s = open(P).read()
if 'EXECBRIDGE-1' in s:
    print('already patched'); sys.exit(0)
bak = P + '.wndrbak_execbridge'
if not os.path.exists(bak): shutil.copy2(P, bak)
old_tick = '''void AUTPlayerController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
'''
new_tick = '''// wndr EXECBRIDGE-1
#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#endif
static FCriticalSection GWndrExecLock;
static TArray<FString> GWndrExecQueue;
extern "C"
#if defined(__EMSCRIPTEN__)
EMSCRIPTEN_KEEPALIVE
#endif
int wndr_exec(const char* Cmd)
{
	if (!Cmd) { return 0; }
	FScopeLock L(&GWndrExecLock);
	GWndrExecQueue.Add(UTF8_TO_TCHAR(Cmd));
	return GWndrExecQueue.Num();
}

void AUTPlayerController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (IsLocalController())
	{
		TArray<FString> Cmds;
		{ FScopeLock L(&GWndrExecLock); Cmds = MoveTemp(GWndrExecQueue); GWndrExecQueue.Reset(); }
		for (const FString& C : Cmds)
		{
			const FString R = ConsoleCommand(C, true);
			UE_LOG(UT, Warning, TEXT("WNDR EXEC '%s' -> '%s'"), *C, *R);
		}
	}
'''
assert s.count(old_tick) == 1
s = s.replace(old_tick, new_tick)
open(P, 'w').write(s)
print('patched OK')
