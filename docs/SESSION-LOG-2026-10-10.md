# UT4 WebGPU — session log (2026-10-10 evening → 2026-10-11) 

## Where we picked up
Prior session froze (suspected: too many screenshot PNGs read into context). All
state was recoverable from RESUME.md + boot logs, so we resumed from disk.

## State at pickup
- Server: `serve_local.py 8091 "/Volumes/ExFAT 1TB/ut4-webgpu/ut-serve"` — healthy.
- Brave+CDP alive since 18:49 on profile `/tmp/brave-live-cdp` (many boots old).
- ut-boot12.log = **degraded boot**: ContentBootstrap 2/3/4 fetch failed 5x each
  ("file could not be read" = cache thrash) → hang-guard booted without them →
  no map (utcontent2 = DM-Chill) → frames advanced 100k+ with
  `passes(bb=0 int=0) draws=0` + SURFDIAG spam. Zombie.

## Boot13 (22:20) — first FULL-CONTENT boot
- Killed zombie, `rm -rf /tmp/brave-live-cdp`, fresh profile.
- **All 7 containers mounted** (global, utcontent, utcontent2..7). Real draws:
  `FRAMESUM f=1..8 passes(bb=31 int=3 depthOnly=1) draws(bb=556..565 int=20)`.
  DEFTIMER #1..#10, 5 bots. Screenshot: DM-Chill interior w/ icicles — RENDERING.
- Froze at frame ~100 right after `RESTARTPLAYER` (respawn) — render thread went
  silent after `SUBMITQ n=300 ENCODER-VALID`; kicks repeated, never recovered.

## Boot14 (23:38)
- Warm reload same tab. All 7 mounted (~80s). DEFTIMER #1..#7 then froze at
  03:42:01 after a `GetSimulationTimeStep max-iterations` warning.
- **Brave window had vanished (0 windows)** — tab hidden+unfocused; freeze
  coincided. AppleScript could not restore (windows:0 stuck); CDP
  Browser.setWindowBounds lied (said normal). Shot-boot14.png: DM-Chill with sky
  light — best-looking frame yet.

## Boot15 (23:55) — fresh Brave process; THE LIMPING-ALONG DATA
- Quit + relaunched. **Reliable focus fix: `node cdp_input.mjs click`** —
  AppleScript activate alone leaves `document.hasFocus()==false`; a synthetic CDP
  click gives focus:true immediately. Also: `[WasmAudio] AudioContext resumed on
  user gesture` only fired after the click.
- All 7 mounted. DEFTIMER #1..#9 so far, 5 bots, draws 556/frame while running.
- Pattern now quantified — game thread stalls every 1-6 min, kicks recover after
  40-345s: frame 41 (40s, recovered), 44 (345s, recovered), 49 (345s? recovered
  via kick), 56 (75s), 57 (180s)... Each stall: `blockSite=0`, proxy 0/0/0 idle,
  `presentedFrame=-1` (counter unwired — Dawn presents DO happen, compositor
  screenshots show evolving frames).
- Real-time per game-second ballooned: DEFTIMER t=8.63 real=1234s, t=9.78
  real=1499s (~2.5 min real per game-second — all spent parked in stalls).
- 64 workers exist (pthreads). Stack sampler (cdp_stacks2.mjs) attaches + pauses
  but gets NO pause events — workers are in uninteruptible waits (likely
  pthread_cond wait not exposing JS frames, or pause doesn't reach wasm workers).
  Further sampling made things WORSE (Debugger.attach spam may add load) — after
  heavy sampling the stalls got longer. Do NOT hammer workers on the next boot.
- Compositor screenshots during play: shot-boot15.png (red overlay, pre-respawn),
  shot-boot15-live.png (post-respawn, no overlay, view advanced) — rendering +
  camera movement confirmed visually.

## Convergent failure model (boot13/14/15)
Full-content boots run ~7-10 game-seconds (~2-4 real minutes), then the game
thread repeatedly parks in a lost-wakeup wait. Kicks recover it but stalls get
longer; sometimes (boot13) they stop recovering entirely. Not an OOB (boot6's
crash is a separate failure). Proxy diag shows the render side idle during
stalls — the game thread is NOT blocked on GPU work; it's blocked on a task-queue
wakeup that never fires (the OPEN #2 lost-wakeup), and the heartbeat kick only
sometimes re-primes it.

## Tools added (this dir)
- cdp_shot.mjs — compositor screenshot (works even when tab hidden).
- cdp_input.mjs — synthetic click/key + THE focus fix (`click` first after boot).
- cdp_nav.mjs — navigate game tab (warm-cache reboot in place).
- cdp_front.mjs / cdp_vis.mjs — visibility/focus reports.
- cdp_window.mjs — Browser.getWindowForTarget/setWindowBounds inspection.
- cdp_stacks2.mjs — worker stack sampler (works; but workers show "running" —
  pause events not delivered; and sampling seems to aggravate stalls).

## Key facts
- `WebGPUProxyWatchdog` string IS in deployed wasm (cb89a5d5, Aug-20 waitOnId 30s
  watchdog) but never fired → the park is NOT in waitOnId. It's a different wait
  (task-queue pump / frame-sync `frameSync=NNNNN/0` counter is the tell — second
  number stuck at 0).
- nixtop reachable (12h+ uptime — power crashes stopped since Oct 10 morning).
  Build tree + rebuild_wasm3.sh intact; webgpu_proxy.h untouched since Aug 20.
- Boot6-style OOB did NOT reproduce in 13/14/15 (they deadlock instead of crash).

## **THE FIX IS ALREADY IN THE DEPLOYED WASM — SINGLETHREAD (2026-08-19/20)**
- `ut4-webgpu-push` repo on nixtop has the SINGLETHREAD commit series, head 7f4775c:
  - `f11afd9` FScheduler::StartWorkers → **0 LowLevel workers on wasm** (tasks run
    inline; no worker-thread race with render/RHI) — "the exact spawner
    NumberOfWorkerThreadsToSpawn missed".
  - `c438034` force synchronous game-thread async loading on wasm
  - `82ab25b` tick Null OSS async manager inline; `82b3406` clear cross-thread asserts
  - `c8693c7`/`b4bfde6` skip OOB memcpy/memmove (poison src) instead of trapping
  - `7f4775c` pump RHI submit dispatch inline → frame presents
- **VERIFIED IN DEPLOYED WASM** (md5 cb89a5d5, utf-16 strings):
  "WNDR SINGLETHREAD" @189301494, "forced to 0 workers" @189301570, plus
  "WebGPUProxyWatchdog" @189287466. ss-build tree source matches (Scheduler.cpp
  Aug-19 with the 0-worker early return; LaunchSimplyStream.cpp has tick-wide
  delete-fence + AR-fix). So the Aug fixes are LIVE in what we're booting.
- Also in the tree: `RenderingThread.cpp` FFrameEndSync::Sync() loop wakes the
  render thread every iteration (Aug-20 PLATFORM_WASM self-heal) — yet boot15 still
  stalled 40-345s per park. So the lost wakeup is NOT (only) the frame fence —
  something else parks. Candidates: `SleepNoStats` uses
  `__emscripten_atomics_sleep` (fine), or the game thread waits on a non-fence
  primitive (FTaskGraph minor listeners, IO dispatcher, audio).

## Boot15 limping quantified (for the record)
- Stalls→recover cycles: frame 41 (40s), 44 (345s), 49 (345s), 56 (75s), 57 (180s)
  … DEFTIMER t=8.63 at real=1234s; t=9.78 at real=1499s. Kicks DO recover; the
  game limps at ~1 game-second per 2.5 real minutes. Rendering + camera advances
  between stalls (screenshots prove it).
- 64 workers; Debugger.pause on them yields NO pause events (they're in native
  futex/atomics_sleep waits) — CDP stack sampling can't see them. Don't hammer.
- LATE UPDATE (23:55): boot15 STILL limping at t+3551s — DEFTIMER #12 at
  real=3010s, last stall report frame 57 at 04:21 (no new STALLED lines since —
  either it unstalled or the heartbeat's own rAF-driven timer is throttled in the
  hidden window). Game remains alive-but-slow; the whole session it never got past
  ~frame 57. This run is a perfect reproduction bed for the instrumented build.

## Probe-site map (wndr_probe.h) — for instrumenting the unknown park
0 unknown/other | 1 waitOnPipeline | 2 wgpu proxy waitOnId | 3 fsQueue/EmQueue
waitOnId | 4 VFS View::wait_for_ready | 5 INFINITE FEvent::Wait (game thread) |
6 UNTIMED ParkingLot (FMutex/FEventCount/task waits) | 8 MAIN_THREAD_EM_ASM sync
proxy | 9 websocket send sync-proxy. Current stalls report site 0 — none of these
scopes cover the actual park. Sites to add: FTaskGraph FSharedEventRef wait in
RenderingThread Sync loop (covered as 5? apparently not firing → maybe the park is
in a wait NOT wrapped — e.g. FPThreadEvent::Wait timed path, or
FPlatformProcess::SleepInfinite, or the audio mixer's condition wait).

## NEXT (in order)
1. **Name the stall site**: heartbeat says `blockSite=0 (unknown/other)` — the park
   is in a wait NOT covered by probe sites 1-9 (see probe map above). Add
   FWndrProbeScope coverage around the remaining candidates: timed FPThreadEvent
   waits on the game thread, FPlatformProcess::SleepInfinite, audio-mixer condition
   waits, and the FFrameEndSync CompletionEvent->Wait(FTimespan) loop in
   RenderingThread Sync (~line 2537 — it's TIMED so unprobed, but 15-345s stalls
   mean it loops; probe the loop). Rebuild 37s via rebuild_wasm3.sh, redeploy
   wasm+js to ExFAT ut-serve.
2. Cheap same-rebuild experiments: (a) double-kick in wndr_kick_named_threads
   (Render + RHI + Game, twice 1s apart); (b) try EM_TIMING_SETTIMEOUT 100ms main
   loop permanently — different timing may dodge the lost wakeup.
3. After fix: verify weapon + fire (utcontent6/7 mounted; cdp_input click).
4. Sync hardened ContentBootstrap_{1..7}.js to nixtop ut-serve if nixtop serving
   returns (RESUME.md lines 86-88).

## How to run (current, Mac-only)
- server: `python3 serve_local.py 8091 "/Volumes/ExFAT 1TB/ut4-webgpu/ut-serve"` (running)
- browser+logger: `node cdp_logger.mjs "/Applications/Brave Browser.app/Contents/MacOS/Brave Browser" http://127.0.0.1:8091/ 9333 > ut-bootNN.log 2>&1 &`
- warm reboot: attach logger (`cdp_attach.mjs <tabID> 9333`) + `cdp_nav.mjs` new URL
- **focus fix after boot: `node cdp_input.mjs click "" 9333`** (do it once, early)
- fetch "file could not be read" → quit Brave, `rm -rf /tmp/brave-live-cdp`, relaunch
- boot logs: ut-bootNN.log; screenshots: shot-*.png (do NOT read many into context
  — that's what killed the prior session; read one, act, move on).
