# UT4 WebGPU browser — resume notes (2026-10-10)

## What this is
UT4 (UE5.8 port) compiled to wasm64 + WebGPU (clean-room DawnRHI), boots in a browser.
Best deployed build on nixtop: **md5 8f157c3c80090af4ed18bd469cb86a8b** (crash-fix,
survived 7.5 min live gameplay, cleared the old ~2:00 OOB crash).

## Where everything lives
- nixtop build tree: `/mnt/vms/ss-build/UnrealEngine` (+ `ut4-webgpu-push` repo clone)
- nixtop web root: `/mnt/vms/ss-build/ut-serve/` (index.html + ContentBootstrap*.data
  [0=88MB indexes, 1..7=6.3GB eager] + UnrealTournament{.js,wasm64.wasm,wasm64.data})
- Server: `python3 serve.py 8091 .` in that dir (COOP/COEP + HTTP range, listens 127.0.0.1)
- Mac runner: this dir's `cdp_logger.mjs` (headed Brave + CDP console logger)
- SSH: `nixtop` alias → 192.168.68.157 (lucaspick)

## How to run (Mac)
1. On nixtop: `cd /mnt/vms/ss-build/ut-serve && nohup python3 serve.py 8091 . > /tmp/ut-serve.log 2>&1 &`
2. Tunnel: `ssh -N -L 8091:127.0.0.1:8091 nixtop &`
   ⚠️ As of the Oct-8 NixOS rebuild, sshd has `AllowTcpForwarding no` → tunnel fails
   with "administratively prohibited". Fix: add
   `services.openssh.settings.AllowTcpForwarding = true;` to
   `~/nix-config/hosts/nixtop/configuration.nix` + `nixos-rebuild switch` (needs sudo pw),
   OR bypass with: run browser ON nixtop, or `socat`/reverse-ssh trick.
3. On Mac: `node cdp_logger.mjs "/Applications/Brave Browser.app/Contents/MacOS/Brave Browser" http://127.0.0.1:8091/ 9333 > ut-boot.log 2>&1 &`
   (Brave 154 has JSPI+Memory64; keep flags as in the script.)
4. Game auto-boots DM-Chill, 1 bot, quickstart. Loading bar → menu/3D.

## State at pause (2026-08-24, prior session)
- Game boots, menu renders, live gameplay reached ~27 FPS, crash=0 (build 8f157c3c).
- OPEN #1 (the reason we paused): **6.3 GB content eagerly preloaded** into the 16 GB
  wasm heap (ContentBootstrap_1..7 = MEMFS-resident). Lazy path SCAFFOLDED but bypassed:
  package 0 (88 MB: engine config + shader cache + AssetRegistry + .utoc indexes) is
  deployed; `regen_lazy_pkg0.sh` documents design = fetch big .ucas on demand via
  createLazyFile HTTP range. Next agreed spike: boot with package 0 ONLY (drop the
  <script> tags for 1..7), watch whether IoStore reads resolve via createLazyFile or
  break. If broken → failure mode informs the OPFS (sync access handles) implementation.
- OPEN #2: game-tick freeze ~3 min in (WebGPU proxy-thread livelock; fix WRITTEN in
  webgpu_proxy.h but BUILD-BLOCKED: header-only precompiled module, no source .o on box).
- OPEN #3: dummy-gray materials (79M NULLHANDLE→dummy during streaming = untextured).

## Session resume 2 — FINAL STATE (2026-10-11 01:10 CDT): LIVE GAMEPLAY ON MAC
Arc of the session (all times CDT):
1. 16:05 Root-caused the 12:55 boot Fatal: standalone `main2()` never called
   `FCommandLine::Set()` -> early AR-fix probe -> PathPak `FCommandLine::Get()`
   -> Fatal. Fix applied in LaunchSimplyStream.cpp (+ Misc/CommandLine.h include),
   backup LaunchSimplyStream.cpp.wndrbak_cmdline; rebuilt via rebuild_wasm3.sh
   (new wasm md5 cb89a5d5), deployed to nixtop ut-serve + ExFAT.
2. 16:30+ Boot then STALLED at `Backend->Mount utcontent4.utoc` forever.
   Root cause #2: the 16:10 link REGENERATED UnrealTournament.js and WIPED the
   `__emscripten_atomics_sleep` JS patch (stub=true -> HttpManagerThread aborts
   ~200ms in -> anything needing the HTTP thread hangs). Re-applied
   patch_atomics_sleep_js.py on nixtop+local AND appended patch+deploy steps to
   rebuild_wasm3.sh so it can't be lost again.
3. 17:40+ Next boot: IoStore mounts raced ahead but most ContentBootstrap_1..7
   fetches failed ("Array buffer allocation failed" then "file could not be
   read") -> hang-guard dropped them -> GDefaultMaterials assert. Fixes:
   (a) serialize the 7 big package fetches through a global FIFO
   (window.__wndrPkgChain) — serialize_bootstraps.py, applied to all 7
   ContentBootstrap_N.js on ExFAT; (b) fresh Brave profile (old /tmp/brave-live-cdp
   had ~7GB of thrashed cache entries; cleared it).
4. 23:49 COLD boot on fresh profile: utcontent1,2,4,5,6,7 mounted (utcontent3
   failed fetch — only miss), NO assert, engine init completed, gameplay reached:
   DEFTIMER ticks, frame 745+, bots=5, ~27s real-time per game-minute. The known
   game-thread lost-wakeup stalls (15-25s) fire every ~1-2 min and the
   WndrHeartbeat kick RECOVERS them (18 stalls / 18 recoveries, 0 crashes) —
   that's OPEN #2's mitigation working; still want the real webgpu_proxy.h fix.
   Screenshot: shot-live-fix.png (DM-Chill interior + ice details; lightmaps dim
   because utcontent3/AssetRegistry.bin missing -> AR-fix couldn't resolve).
5. 00:57 boot6 (fresh profile, hardened retries: 8 net attempts x8s backoff,
   6 alloc attempts x10s): **ALL SEVEN ContentBootstrap containers mounted**
   (utcontent1-7, only the nonexistent utcontent8-12 fail as expected), gameplay
   reached again. New known issue: a render-worker OOB ("memory access out of
   bounds", pthread 0x44afe928) fires mid-draw at frame ~23 and the GAME THREAD
   FREEZES right after (DEFTIMER/MDCDRAW stop at frame 23; JS heartbeat +
   compositor keep running — screenshot shows the frozen last frame). This is
   the next session's target: the OOB stack lands in the mesh-draw path
   (MDCDRAW pass=20, CLIPPROBE prims=397) and matches the class of bug fixed by
   commits b4bfde6/c8693c7 on nixtop ut4-webgpu-push (compiler-generated
   struct-copy memory.copy OOB in base pass). Suggested approach: SAFE_HEAP=1
   + -g2 worker-stack capture on pthread 0x44afe928, or the WRKSTK guard from
   21914ed. Note boot3 (utcontent3 missing) ran 35+ min with only recoverable
   stalls — the OOB appears related to richer geometry from utcontent3
   (or its AssetRegistry-resolved meshes).
   Screenshots: shot-live-fix.png (boot3, ut3 missing), shot-boot6.png (boot6,
   all containers — geometry richer: sky light + more surfaces lit).
6. Retry hardening lives in /Volumes/.../ut-serve/ContentBootstrap_{1..7}.js
   (wndr-retry 8x8s / alloc 6x10s). The nixtop ut-serve copies do NOT have the
   serialization/hardening — sync if nixtop serving ever returns.
7. 01:10-02:50 follow-ups (user asked "can we open it / see gun fire"):
   - Boot7/8/9: intermittent big-alloc failures kept dropping 3-5 containers
     (pattern: whichever are in flight when the tab's memory ceiling hits).
     pak-list census on nixtop: utcontent6 = 3,606 weapon entries (UTWeap_*),
     utcontent7 = 4,326; utcontent2 has the DM-Chill map. So dropped 6/7 =
     no gun/HUD/fire (WeaponAtlas01 + UTDroppedAmmoBox CDO errors in log).
   - Fix attempt: priority order [6,7,1,5,4,2,3] via window.__wndrPkgOrder
     (prioritize_bootstraps.py) — weapons fetch first while MEMFS is small.
     GOTCHA: serve_local.py was heuristically caching .js, so reloads ran the
     STALE scripts; added `Cache-Control: no-cache` for .js/.html in
     serve_local.py and restarted it (PID changes; relaunch with
     `python3 serve_local.py 8091 "/Volumes/ExFAT 1TB/ut4-webgpu/ut-serve"`).
   - Boot10 (02:32, all fixes live): utcontent 6+7+5 mounted (weapons IN),
     map loaded, frames advanced to ~999, NO worker OOB. Page currently
     "frozen" only because the tab is hidden/occluded (rAF throttled;
     visibilityState=hidden). Game resumes when the window is focused.
   - The DOM loading overlay may linger after first present (cosmetic).
   NEXT: with window focused, verify first-person weapon + fire (synthetic
   click via CDP Input.dispatchMouseEvent if needed). Then the worker OOB from
   boot6 (frame ~23, all-containers) is the remaining crash to chase.

How to run now (Mac, no nixtop needed at runtime):
- server: `python3 serve_local.py 8091 "/Volumes/ExFAT 1TB/ut4-webgpu/ut-serve" &`
- browser: `node cdp_logger.mjs "/Applications/Brave Browser.app/Contents/MacOS/
  Brave Browser" http://127.0.0.1:8091/ 9333 > ut-boot.log 2>&1 &`
- attach-to-existing-tab logger: `node cdp_attach.mjs <tabID> 9333`
- cdp_logger.mjs kills any stale profile? NO — it reuses /tmp/brave-live-cdp;
  if boots start failing with "file could not be read": `rm -rf /tmp/brave-live-cdp`
  and relaunch (cache thrash after ~2-3 cold boots).

## Session resume 2 (2026-10-10 ~16:15 CDT) — PathPak CommandLine crash ROOT-CAUSED + FIXED
- The 12:55 boot (first with the 12:30 wasm) crashed after "Running...":
  `Fatal [CommandLine.cpp:63] Attempting to get the command line but it hasn't
  been initialized yet.` at `EnsurePathPakMounted() <- PathPak_FileExists()
  <- SIMPLYSTREAM_Init() <- main2`.
- ROOT CAUSE: the AR-fix block added 2026-10-10 at the top of `SIMPLYSTREAM_Init()`
  (early `IFileManager::Get().FileExists(AssetRegistry.bin)` probe, BEFORE
  PreInit) reaches the IAS PathPak wrapper -> `EnsurePathPakMounted()` ->
  `FCommandLine::Get()`. But the wndr standalone path (`main2()` in
  LaunchSimplyStream.cpp) writes the `GCmdLine` global directly and never calls
  `FCommandLine::Set()` (that normally happens much later inside
  LaunchEngineLoop::PreInit) -> `bIsInitialized==false` -> UE_LOG Fatal.
- FIX APPLIED (16:05): `LaunchSimplyStream.cpp` main2() now calls
  `FCommandLine::Set(GCmdLine)` right after building the cmdline (+ include
  Misc/CommandLine.h). Backup: `LaunchSimplyStream.cpp.wndrbak_cmdline`.
- Rebuilt via `rebuild_wasm3.sh` (rc=0, 37 s — Launch-only incremental), new wasm
  md5 cb89a5d54d535c6f31a051b8709e584a deployed to nixtop ut-serve (wasm+js,
  16:11).
- ALSO on local Mac serve tree (ExFAT): the earlier session's resilient
  ContentBootstrap_{1..7}.js + ContentBootstrap.js (wndr-retry/hang-guard,
  13:44-13:55) are DEPLOYED but NOT yet boot-tested; they fetch with
  `cache:'force-cache'` (warm-cache reboots took ~16 s in the 12:55 run).
- Serving setup changed since RESUME.md was written: local Mac server
  `serve_local.py 8091 "/Volumes/ExFAT 1TB/ut4-webgpu/ut-serve"` (PID may differ)
  + local Brave+CDP (port 9333, user-data /tmp/brave-live-cdp). No SSH tunnel
  needed at all now. nixtop is only the build box.
- NEXT: ~~pull new wasm+js to ExFAT serve tree~~ DONE (16:11 local). Boot-tested
  16:12→18:23 (t+130min): **CommandLine fatal is GONE** — boot proceeds through
  AR-fix probe (`AssetRegistry.bin FileExists=0` — clean, no Fatal), PreInit runs,
  IoStore mounts global/utcontent/2/3 (ok=1 each, ~1.6k packages per container),
  then **STALLS at `Backend->Mount utcontent4.utoc`** for 2h+ with zero net
  failures, zero retries, main thread responsive, MEMFS files all present
  (utcontent4.ucas openable), heap steady 6.8 GB. Engine last line: "calling
  Backend->Mount /UnrealTournament/Content/Paks/utcontent4.utoc" — no
  "returned"/FAILED line. Same load worked in nixtop boots (Aug) — suspect
  SyncFetch/raw-fopen path waiting on an event that never fires in this
  local-serve env (or the missing utcontent8..12 .utoc — the mount list expects
  them but the local tree only has global+utcontent..7 = 6.3GB packages; ut8-12
  were never part of this bootstrap set... CHECK: does ut-serve on nixtop have
  utcontent4..7 mounting fine? Compare against an Aug nixtop boot log).
- NEXT-2: investigate utcontent4 mount stall (compare nixtop Aug boot log
  /mnt/vms/ss-build/ut-local-webgpu-console.log; maybe strace the wasm read via
  CDP Debugger.pause on the worker to get a stack).

## Fresh-boot hang mode (2026-10-10 ~18:00, Mac) — READ BEFORE RETRYING
- Symptom: loading bar stalls (~96%, "ETA 2s") with `still waiting on run
  dependencies:` naming 2–3 ContentBootstrap_*.data. Cause: during the 10-min
  eager download, one package's final `new Uint8Array(chunks)` throws
  `RangeError: Array buffer allocation failed` (peak transient use ≈ 2× package
  size + all others resident ≈ 12+ GB). The bootstrap's `loadPackage` has NO
  catch → its `addRunDependency` is never removed AND the fetchPromise rejection
  is swallowed as an unhandledrejection → boot waits forever. Server-side
  trigger visible as `BrokenPipeError`/`Errno 55 No buffer space available` in
  /tmp/ut-serve-local.log (client aborted mid-send under memory pressure).
- CDP recovery attempts FAILED (page unrecoverable): manually fetching +
  `FS_createDataFile` + `removeRunDependency` desyncs the counter (goes -1,
  assertion aborts on next remove), and `Module._main2` errors "called before
  runtime initialization" (runtime init ran on a pthread). Do not sink more
  time into in-page repair.
- RELIABLE RETRY: just reload the page fresh (Page.navigate with new ?r=).
  Every failure happens near the END (alloc peak), so a retry reuses HTTP cache
  only if the server sends cacheable headers — it does for .data (immutable) —
  but emscripten still re-downloads (preloadResults.fromCache=false). Accept
  the ~10 min redownload; ~72% free RAM at start makes success likely.
- REAL FIX (next code session): patch ContentBootstrap_{1..7}.js `fetchRemotePackage`
  to (a) retry with backoff on fetch/alloc failure and (b) on final failure,
  call `removeRunDependency(dep)` + `alert` so boot proceeds without that
  container (IoStore will just miss those paks) instead of hanging forever.
  Optionally stitch chunks into one buffer with `new Blob(chunks).arrayBuffer()`
  (off-JS-heap) to halve peak JS memory. Also consider raising
  `--js-flags=--max-old-space-size` on the Brave launch line.

## nixtop restart mystery (2026-10-10)
- Power loss Oct 9 ~16:39 CDT (journal ends mid-write, no panic/OOM/shutdown; laptop
  battery "Charging" now). Stayed off ~15 h, booted 07:43 Oct 10, then dropped off
  network again ~07:52 (2nd crash today). Physically unreachable — likely power/battery.
- Unrelated pre-existing: mongo-express container crash-loops (mongo deps not up).
