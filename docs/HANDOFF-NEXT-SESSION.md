# UT4 WebGPU — HANDOFF (2026-10-11, for next Claude session)
Start here. Read HANDOFF-NEXT-SESSION.md first.

## Project
UT4 (UE 5.8 SimplyStream platform port) compiled to wasm64 + WebGPU (clean-room
DawnRHI), boots and runs DM-Chill in a browser. Local-only runtime on the Mac
(nixtop is the build box). Everything below assumes the Mac context.

## Runtime state RIGHT NOW
- **Boot21 is alive in headed Brave** (window on screen): DM-Chill running,
  1 player + 5 bots, DEFTIMER #8 t=8.59 real=78.8s. All 7 content containers
  mounted. Render is healthy (556 draws/frame when unstalled).
- Server: `serve_local.py 8091 "/Volumes/ExFAT 1TB/ut4-webgpu/ut-serve"` (running).
- Brave+CDP port 9333, logger attached → `ut-boot21.log` in this dir.
- Deployed wasm: md5 f49105b5 (includes AR-fix-2/2c/2d/2e/2f diagnostics + fix attempts).

## THE OPEN TASK: map renders way too dark
User: "the map is still way too dark, it won't be complete as long as it is that dark."

### Root cause chain (CONFIRMED this session)
1. Premade AssetRegistry.bin (7.9 MB) IS in MEMFS at `/UnrealTournament/AssetRegistry.bin`
   (verified from JS: `FS.stat` → 7981528 bytes; and from the game thread: raw
   `stat()` succeeds — `[wndr AR-fix-2] probe ... rawstat=0 size=7981528`).
2. Engine's premade-AR loader fails twice:
   - **Find fails**: `FPreloader::TrySetPath()` → `IFileManager::FileExists("../../../UnrealTournament/AssetRegistry.bin")`
     returns 0 (`IFile=0`) — BaseDir/relative-collapse semantics in
     `FUnixPlatformFile::FileExists` break the relative path.
   - **Open fails**: after our fix-2 finds the path, `OpenRead()` on the same
     absolute path returns a NULL handle (`[wndr AR-fix-2e] OpenRead(...) handle=NULL`)
     even though raw `stat()` succeeds 130ms earlier on the same thread.
     LoadResult went 1 (NotFound) → 2 (FailedToLoad) after fix-2.
3. Consequence: GUID-only cooked lightmap/shadowmap refs never resolve →
   static world geometry binds a DUMMY (zero) PrecomputedLightingBuffer →
   the map renders with only the injected flat ambient → near-black interiors.
   Log evidence: 60× `WNDR WALLLIGHT DUMMY-UB name=PrecomputedLightingBuffer`.
4. Measured darkness: 74% of frame < 32/255 luminance, mean 36/255.

### Why `IFile=0` when rawstat works
`FUnixPlatformFile::FileExists` → `NormalizeFilename` → (this build has NO wasm
branch in that function — the `/Engine/Binaries/SimplyStream` rewrite was never
compiled in; verified: no `[wndr-diag] Normalize in=` string in the deployed wasm)
→ `ConvertRelativePathToFull` with `FSimplyStreamPlatformProcess::BaseDir() == ""`
(SimplyStreamPlatformProcess.cpp:28-31!) → path stays relative
`../../../UnrealTournament/...` → UE's `CollapseRelativeDirectories` refuses to
collapse leading `..` (bAllCollapsed=false path in PathViews.cpp:690-693 returns
early) → the mangled relative goes to stat → fails. (The raw-stat probe that
SUCCEEDS in main2 uses `stat()` directly with cwd=/ — POSIX resolves it; UE's
wrapper mangles it first.)

### THE REMAINING MYSTERY (pick up here)
`open()` fails where `stat()` succeeds on the SAME thread + path:
- fix-2f added an errno log on direct-open failure in
  `FUnixFileRegistry::PlatformInitialOpenFile` (UnixPlatformFile.cpp) — **but the
  `[wndr AR-fix-2f] direct open(...) FAILED errno=N` line NEVER appeared** in
  boot21's log, while `[wndr AR-fix-2e] OpenRead(...) handle=NULL` DID.
  Interpretation: the failing OpenRead call is NOT reaching
  `FUnixFileRegistry::PlatformInitialOpenFile` at all — i.e.
  `IPlatformFile::GetPlatformPhysical()` inside
  `OpenReadStateFileArchive` (ARLoader.cpp:~475) is NOT returning the
  FUnixPlatformFile we expect, OR there are two singletons / the platform-file
  chain on wasm routes OpenRead elsewhere (CachedReadFile wrapper was active
  earlier in boot: "platform file BEFORE force = CachedReadFile").
- Next concrete step: log the concrete `GetName()` of the platform file used in
  `OpenReadStateFileArchive` and inside `FUnixPlatformFile::OpenRead` itself,
  and/or call `open()` RAW in `OpenReadStateFileArchive` (bypass IPlatformFile
  entirely, wrap fd in a minimal IFileHandle or use `fopen`/`FILE*` + a tiny
  FArchive reader). The simplest robust fix: **read the AR bytes with raw
  POSIX (`fopen`/`fread`) in `OpenReadStateFileArchive` on PLATFORM_WASM** and
  hand them to `FLargeMemoryReader` — no platform-file layer involved at all.
  That file is engine source we CAN edit (AssetRegistry/Private/ARLoader.cpp).
- Alternatively (fallback visual fix): raise the ambient knobs in
  DawnRHI — `GetDawnAmbientLightmapBuffer` A=0.59 (DawnCommandContext.cpp:~333),
  AMBFLOOR albedo*0.12 (DawnDynamicRHI.cpp:~1060), FOG_INSCATTER_SCALE 0.12
  (DawnDynamicRHI.cpp:~2909). These are the "make it brighter without real
  lightmaps" levers. The user wants the map COMPLETE, i.e. real lightmaps —
  so fix the AR load first, knobs only as last resort.

## Patches applied this session (all on nixtop /mnt/vms/ss-build/UnrealEngine)
1. `Engine/Source/Runtime/AssetRegistry/Private/ARLoader.cpp`
   - AR-fix-2: `TrySetPath()` probes absolute MEMFS paths
     (`/UnrealTournament/AssetRegistry.bin`) via `IPlatformFile::GetPlatformPhysical().FileExists`
     — this MAKES TrySetPath succeed (log: "premade AR found via MEMFS path").
   - AR-fix-2c: probe diagnostics (IFile= / rawstat= lines).
   - AR-fix-2d: `OpenReadStateFileArchive` prefers `GetPlatformPhysical().OpenRead` on wasm.
   - AR-fix-2e: OpenRead/Size/Read step diagnostics (OpenRead handle=NULL proven).
   - Backup: `ARLoader.cpp.wndrbak_arfix2`
2. `Engine/Source/Runtime/Core/Private/Unix/UnixPlatformFile.cpp`
   - AR-fix-2f: errno log on direct-open failure in `PlatformInitialOpenFile`
     (never fired — the null handle comes from somewhere else; see mystery above).
   - Backup: check for `.wndrbak` siblings; this file already had wndr wasm edits.
3. Patch scripts (this Mac dir, also copied to nixtop /mnt/vms/ss-build/):
   fix-arloader-memfs-path.py, fix-arloader-openread-physical.py,
   fix-arloader-diag.py, fix-arloader-physical-open.py, fix-arloader-diag2.py,
   fix-open-errno-diag.py

## Build & deploy loop (PROVEN, ~4 min end-to-end)
1. Edit source on nixtop (or scp a python patch script + run it).
2. `ssh nixtop 'cd /mnt/vms/ss-build/UnrealEngine && nohup bash /mnt/vms/ss-build/rebuild_wasm3.sh > /dev/null 2>&1 &'`
   - ~40-115s, rc=0 in /mnt/vms/ss-build/rebuild_wasm3.log tail.
   - Script auto-applies patch_atomics_sleep_js.py and copies to nixtop ut-serve.
3. `scp nixtop:/mnt/vms/ss-build/ut-serve/UnrealTournamentwasm64.wasm "/Volumes/ExFAT 1TB/ut4-webgpu/ut-serve/"` (+ UnrealTournament.js)
4. Reboot the tab: `node cdp_nav.mjs "http://127.0.0.1:8091/?r=bootNN" "" 9333`
   then `nohup node cdp_attach.mjs 693BC954 9333 > ut-bootNN.log 2>&1 &`
   (tab id 693BC954… is the current game tab; list tabs via
   `curl -s http://127.0.0.1:9333/json/list`).
5. **Focus fix (required every boot):** `node cdp_input.mjs click "" 9333`
   (AppleScript activate alone leaves document.hasFocus()==false).
6. Verify AR state: `grep "AR-fix-2\|Failed to load premade\|Premade AssetRegistry loaded" ut-bootNN.log`
   Success looks like: "Premade AssetRegistry loaded from '/UnrealTournament/AssetRegistry.bin'"
   and NO "Failed to load premade" error.

## Boot mechanics (known gotchas)
- Fetch failures "file could not be read" = browser cache thrash → quit Brave,
  `rm -rf /tmp/brave-live-cdp`, relaunch via cdp_logger.mjs (10-min re-download).
- The window/tab must stay visible+focused or the game parks (rAF throttle);
  it recovers on focus but stalls leave slow patches.
- The game-thread lost-wakeup stalls (frame ~40-100, kick-recovered) still exist
  (OPEN #2 from RESUME.md) — unrelated to the darkness; boot21 ran to DEFTIMER #8.
- Do NOT read many screenshot PNGs into context (killed a prior session);
  read one, act, move on. Screenshots: `node cdp_shot.mjs out.png "" 9333`.
- 64 workers; CDP Debugger.pause gets no events from them (native futex waits).

## Brightness levers (last resort, DawnRHI, nixtop)
- `DawnCommandContext.cpp` ~line 333: `const float A = 0.59f;` in
  GetDawnAmbientLightmapBuffer — flat ambient for dummy lightmaps
  (exp2(16A-8)*0.6 ≈ 1.63 ambient).
- `DawnDynamicRHI.cpp` ~line 1060: AMBFLOOR `FloorExpr = textureSample(...)*0.12`
  (max() floor per-pixel).
- `DawnDynamicRHI.cpp` ~2909: `DAWNRHI_FOG_INSCATTER_SCALE 0.12`.
- `DawnDynamicRHI.cpp` ~3013+: PREEXPOSE forced to 1.0 (already no-crush).
- FULLBRIGHT gate (`DAWNRHI_FULLBRIGHT`, DawnDynamicRHI.cpp ~875): diagnostic
  albedo-only mode (renders flat, proves textures fine) — do NOT ship.

## Docs in this dir
- **HANDOFF-NEXT-SESSION.md — this file — start here.**
- RESUME.md — original long resume notes (Aug→Oct 10).
- SESSION-LOG-2026-10-10.md — last night's session (boots 13-15, livelock data,
  tool inventory, SINGLETHREAD-fix-already-in-wasm verification).

## History one-liner
Boot12 zombie (cache thrash) → boot13 first full-content boot (all 7 containers,
real draws, DEFTIMER) → boot13/14 froze at frame ~100 (stall not recovering) →
boot15 limps (kicks recover 40-345s stalls) → boot15-morning/16/17+ focused boots
run but the MAP IS DARK → root-caused to premade AssetRegistry not loading →
fix-2 made TrySetPath find it (rawstat works) → OpenRead still returns NULL
(2e proved; 2f errno diag never fired = the failing open isn't in the code path
we instrumented) → NEXT: raw-POSIX read in OpenReadStateFileArchive.

## Boot21 left running (2026-10-11 ~14:50 CDT)
- Boot21 (md5 f49105b5) was left ALIVE in headed Brave: DEFTIMER #8, one
  recovered stall at frame 45. It is a reproduction bed for the next session.
- To kill: pkill cdp_logger/cdp_attach + close the window; server can stay up.
- All session state is in this file + ut-boot17..21.log.

---
## SESSION 2026-10-11 (afternoon) — results, supersedes "THE REMAINING MYSTERY" above
- **AR-fix-3 WORKS** (wasm md5 519b7f1d, deployed): ARLoader.cpp `OpenReadStateFileArchive`
  on PLATFORM_WASM reads via raw fopen/fread → FLargeMemoryReader. Log: "Premade AssetRegistry
  loaded from '/UnrealTournament/AssetRegistry.bin'". Script: fix-arloader-raw-posix.py
  (backup ARLoader.cpp.wndrbak_arfix3). Gotcha: TStringBuilder::GetData() is NOT null-terminated —
  use `*FilePathSZ` (boot22 failed errno=44 with garbage-suffixed path because of this).
- **AR was NOT the darkness cause.** boot23: AR loaded, DUMMY-UB count unchanged (30, capped printf),
  screenshot still near-black (shot-boot23.png). Fable review confirmed: lightmaps come from
  UMapBuildDataRegistry (DM-Chill_BuiltData, which IS packaged), not the AR.
- **Why OpenRead was NULL:** physical platform file is the CLOSED precompiled
  `FSimplyStreamPlatformFile` (Module.wasm_utils.cpp.o), not FUnixPlatformFile — all our
  UnixPlatformFile.cpp diags are dead code. Same layer breaks .ufont / profile / ini reads.
  Broad fix = make GetPlatformPhysical() return the open FUnixPlatformFile.
- **Real darkness suspects (Fable, ranked):**
  1. index.html:89-90 `r.Mobile.Forward.EnableLocalLights=0` + `r.LightCulling.Quality=0` → no
     point/spot lights on ES3_1 forward. **Tried =1 in boot24 → CRASH**: `table index is out of
     bounds` in ExtractShaderParameterResource ← SetShaderParameters ← Dispatch<FLightGridInjectionCS>
     (LightGridInjection.cpp:435-542). Garbage resource pointer → likely param-struct/bindings
     mismatch or an unset RDG UAV (16-bit vs 32-bit culled grid). Reverted to 0
     (backup index.html.bak-locallights0 in ut-serve).
  2. Torch BPs not packaged: 34× BP_WallTorch + 3× BP_BrazierFire "Could not find class object"
     (each has a PointLight); ~200 "dependent package ... not available".
  3. ES3_1 needs LQ lightmaps (FLightMap2D Textures[1]); unverified whether bakes have them.
- **Tooling gotcha:** cdp_nav/cdp_input with "" tab prefix can hit the USER's Brave tabs when they
  browse in the CDP Brave. ALWAYS pass the game tab id prefix explicitly.

## utcontent8 — DM-Chill's excluded deps (2026-10-11 ~15:50)
- ROOT CAUSE of the missing torches/~200 packages: `make_iostore_onemap.sh` EXCLUDES
  Environments/{Outside,Liandri,Industrial,...}; DM-Chill's closure (chill_closure.txt) has 376
  packages there (279 Outside incl. FX/Fire/BP_WallTorch + BP_BrazierFire, 67 Liandri).
- FIX: new container `ut-iostore8/utcontent8` (943 files, 611 MB) built with ONLY those files;
  regenerated global.utoc/.ucas is byte-identical to ut-iostore's (consistent). Packaged as
  ContentBootstrap_8.{data,js}; _8.js derived from patched _7.js (retry/serial/priority patches);
  priority order is now [6,7,1,5,4,2,3,8] in all loaders; index.html loads ContentBootstrap_8.js.
  Backups of pre-change loaders + index.html: "/Volumes/ExFAT 1TB/ut4-webgpu/bak-pre-ut8/".
- boot25 result: BP_WallTorch errors 986→0, "Could not find class" 37→0, "not available" 400→19.
  Map shows more geometry but still dark (torch lights need local lights → LG crash).
- nixtop gotcha: nix-ld no longer active → UnrealPak was patchelf'd with the nix glibc interpreter
  (original saved as Engine/Binaries/Linux/UnrealPak.orig-interp).
- LG-fix-1 (in progress): ShaderParameterStruct.cpp skips NULL resource params on wasm
  (fix-shaderparam-null-skip.py, backup .wndrbak_lgfix1) to survive FLightGridInjectionCS.

## SESSION 2026-10-11 (evening) — texture references, bundle 9, translucency crash

### Root cause of the black world (found via Fable)
DawnRHI never resolved `FRHITextureReference`. Lightmap/shadowmap cluster textures and material
UB textures are bound as references (`SceneManagement.cpp` LightMapTexture->TextureReference,
`MaterialUniformExpressions.cpp` TextureReferenceRHI). `static_cast<FDawnTexture*>` on a reference
read `->View` past the object -> null/garbage -> black dummy (white-fill of dummies is disabled).
- **TEXREF-fix-1** (`fix-dawn-texref-resolve.py`, backups `.wndrbak_texref`): `DawnResolveTex()` in
  DawnCommandContext.cpp, used in TryUBTable UBMT_TEXTURE, LMMISS dim probe, direct EType::Texture
  bind (null -> `continue` -> dummy-fill); SRV creation in DawnDynamicRHI.cpp resolves too.
  Logs `WNDR TEXREF n= resolved= null=`.
- boot31 (wasm 02e1ff81): TEXREF 360000/360000 resolved; LMTEX dummy 52% -> ~3%;
  real textures visible for the first time (shot-boot31.png, mean 18/255 vs 11-13). Still dim.
- Ruled out: GlobalShaderCache-None.bin == cooked SP_WEBGPU_ES31 cache (md5 identical, wasm fallback);
  LQ lightmaps present in DM-Chill_BuiltData; HDR/exposure match stock.
- Still open (needs recook): DawnShaderCompiler emits no UB param for resource-only UBs
  (LightmapResourceCluster) -> LM-fix-2 association workaround. Fix: AddParameterAllocation for UBs
  whose `<UB>_<member>` texture/sampler names are reflected.

### Bundle 9 (18 missing DM-Chill deps)
Package IDs resolved with a CityHash64 hasher built from UE's CityHash.cpp (nixtop /tmp/pkgidhash/pkgid,
reads names on stdin; FPackageId = CityHash64(lowercase UTF-16 name)). The 18 (castle trims, decals,
moss tex, VF_RandomNoise_01_90, Retro_15 LUT) were cooked but missing from chill_closure.txt.
`/mnt/vms/ss-build/ut-iostore9/utcontent9` (24 MB) -> ContentBootstrap_9.{data,js}; index.html loads it
after _8; pkg order [6,7,1,5,4,2,3,8,9] in all loaders. Backups: `bak-pre-ut9/`. boot31: 18 -> 2 missing.

### Translucency crash (pre-existing, intermittent)
"Pthread ... sent an error! table index is out of bounds" right after `MDCDRAW pass=20`
(EMeshPass 20 = TranslucencyAll) in boot27, boot30, boot31 -> game thread then stalls forever.
First seen with bundle 8 (torches/particles). Likely explains boot30 "self reloads".
Hunt in progress: toolchain now adds `--profiling-funcs` for non-shipping (`.wndrbak_crashnames`),
and `patch_worker_errstack_js.py` (hooked into rebuild_wasm3.sh) logs `WNDR WORKER-ERR <stack>`
from pthreads. Boot32 should capture a named stack.

## SESSION 2026-10-12 (day) — speed, HUD, fonts, skinning
All fixes are python patch scripts in this dir (scp to nixtop:/mnt/vms/ss-build, run, rebuild_wasm3.sh).
Each makes a `.wndrbak_<tag>` backup next to the patched file.

### Results
- Crash + 18-95 s "walk" freezes: ONE bug. `StaticUBsByName` held RAW UB pointers forever; freed
  static UBs -> `DUB->ShaderName + "_"` copied garbage-length strings (~1 s memmove/memset per draw)
  or trapped (memcpy 1.76 GB). Fix `fix-dawn-staticub-hold.py` (STATICUB-hold-1 ref-holding map +
  REGLOCK-1 lock on GDawnLiveHandles/GDawnUBByBuffer). Game time now ~= real time, 0 stalls.
- `fix-dawn-bindfast.py` (BINDFAST-1): no FString temporaries in TryUBTable/owner prefix match,
  one ResTable lock per FlushPendingBindGroups. 1.6 -> 6.3 fps.
- `fix-dawn-bgcache.py` (BGCACHE-1) bind-group cache: REVERTED (7% hit rate; engine makes new
  single-frame UBs every frame; slower). Backups `.wndrbad_bgcache` keep the tried version.
- HUD invisible: transient $Globals UBs (RHISetShaderParameters) never registered as live handles
  -> NULLHANDLE swapped them for the ZERO dummy -> canvas PS alpha = dot(tex, ReplicateAlpha=0).
  Fix `fix-dawn-globals-livehandle.py` (GLOBALSLIVE-1). Affects EVERY loose-param shader.
- Fonts: 20 .ufont files were never shipped. New bundle ContentBootstrap_10.{js,data} (8.3 MB,
  Engine/Content/EngineFonts/Faces + UT RestrictedAssets/{UI/,}Fonts), load order
  [6,7,1,5,4,2,3,8,9,10] in all loaders, index.html includes _10 (backups bak-pre-ut10/).
  ONLY on the Mac serve dir so far (not copied to nixtop ut-serve).
- UB naming: `fix-dawn-ubname-metadata.py` (UBNAME-meta-1) uses the engine-registered
  ShaderVariableName (GPUSkinVFBase, LocalVF, PrimitiveFade, PrimitiveDither, SpeedTreeData...)
  instead of stripping the struct type name; those UBs were UBDROPped before. ~6 -> ~20 fps in
  simple views (LocalVF now binds).
- Skinning: `fix-dawn-skinloc.py` (SKINLOC-1) remaps GPU-skin VS inputs: DXC numbers inputs in
  HLSL declaration order (0,1,2,3,14,4,15,5,...) and WGSL loses ATTRIBUTEn. Bone matrices now
  resolve 100% (BONEDIAG). Characters/1P weapon still UNCONFIRMED (user: weapon not visible).
  Proper fix = cooker: set SPIR-V input Location = ATTRIBUTE index before Tint (needs re-cook).
- Build box: clang/wasm tools lost libz (GC'd nix store path in their RPATH). Repointed 31 ELF
  rpaths to /nix/store/wgw25d...-zlib-1.3.2 and pinned gcroots /mnt/vms/ss-build/gcroot-{zlib,gcclib,glibc}.

### Diagnostics still compiled in (remove when done)
RHISTAGE-probe-1/2 (stage store only now; exports wndr_rhi_stage/_bind_calls/_stage_ms),
HUDDIAG-1..7, SKINDIAG-1, SKINWGSL-dump-1, BONEDIAG-1. Pollers: cdp_stagepoll2.mjs, cdp_pump.mjs.

### Open
1. 1P weapon invisible; characters reported as "blocks" (pre-SKINLOC). Verify visually.
2. Texture flashing (user report; not yet investigated).
3. Match-end crash: `Assertion failed: NextProxy == nullptr (Archive.cpp:171)` at WaitingPostMatch.
4. Blocky lightmaps. 5. fps: per-draw bind walk + -O1/ASSERTIONS=1 link flags (toolchain .cs).

## SESSION 2026-10-12 (afternoon) — characters animate, weapon visible
- ROOT CAUSE (invisible weapon / statue characters): UE 5.8 default GPU-skin path allocates bone
  transforms in the Skinning *scene extension* (compute-built TransformDataBuffer). That data is wrong
  on DawnRHI. Fix: `-ini:Engine:[ConsoleVariables]:r.GPUSkin.BoneTransformAllocationMode=0` (index.html)
  -> per-VF CPU-uploaded bone buffers. Verified boot63: bots animate, Enforcer shows in first person.
  (Proper fix later: find why the scene-extension compute upload/scatter fails on WebGPU.)
- SNORMNORMAL-1: DawnResources.h VET_PackedNormal -> Snorm8x4 (was Unorm8x4). Lighting much better.
- EXECBRIDGE-1 (UTPlayerController.cpp): `node cdp_uexec.mjs "<console cmd>" <tab> 9333` runs any console
  command (e.g. "BehindView 1", cvars). Diagnostic, keep for now.
- SKINIDENT-1 (DawnDynamicRHI.cpp): `?skinident=1` -> -WndrSkinIdent -> identity bones (bind pose). Diagnostic.
- Brave debug instance had to be restarted (renderer+GPU process wedged after killing a hung tab).
  Game tab id is now 14C6C086 (was 3402B27F). Profile /tmp/brave-live-cdp untouched.
- New helpers: cdp_rawnav.mjs, cdp_raw.mjs, cdp_sess.mjs (browser-session eval), cdp_browser.mjs.
- MODKEYS-1 (SimplyStreamPlatformInput.cpp): DOM Shift/Ctrl/Alt/Win -> LeftShift/LeftControl/LeftAlt/LeftCommand,
  +256 -> Right*. UE has no generic "Shift" FKey, so UT's One Tap Dodge (LeftShift) never fired.
- CMDLINE-1 (LaunchSimplyStream.cpp): GCmdLine 2048 -> 16384. index.html args hit ~2.1K -> Strcat overflow ->
  boot OOB in main2 (caused by adding the BoneTransformAllocationMode arg).
- STICKYUB-1 (DawnCommandContext.cpp/.h): THE TEXTURE-FLICKER FIX. UE's mesh state cache elides re-sending an
  unchanged UB within a PSO; our per-draw PendingUBRefs.Reset() (2026-08-20 perf) lost it -> lightmap cluster
  fell to dummy/other object's (~4% of LM binds). Now StickyUBBySlot[(freq<<16)|slot] seeds each draw, reset
  on PSO change. LMTEX dummy 4% -> 0; frame diffs stable (boot66).
- DUMMYHIST-1: remaining dummy textures = PositionTexture, MaterialVS_Texture2D_* (VS material textures),
  SceneDepth/CustomDepth in base pass. Next candidates for visual bugs.

## SESSION 2026-10-12 (late afternoon) — perf push (heavy view 3.6 -> ~15 fps)
- F32FILTER-1: request 'float32-filterable'; RGBA32F on filterable slots allowed (GPU sprite PositionTexture
  -> Link Gun plasma etc. visible).
- MATVS-1: MaterialVS_<Member> resolved from the draw's full Material UB (VS material textures: tree wind/WPO).
- BINDLEAN-1: per-PSO cached layout; per-draw diagnostics in FlushPendingBindGroups off (49us -> 9us/draw).
- LINKOPT-1 (SimplyStreamToolChain.cs): Development link -O2, ASSERTIONS=0 (wasm 330MB -> 223MB).
  UBT recompile needed: Roslyn bincore/{csc,vbc,VBCSCompiler} patchelf'd to nix glibc (+.wndrbak_orig),
  rebuild_wasm3.sh exports DOTNET_ROOT.
- patch_async_stdout_js.py (in rebuild_wasm3.sh): pthread stdout/stderr posted to main fire-and-forget
  (was sync proxy = 13% of frame). Main side calls Module.print/printErr (out/err skip Module.print with
  ASSERTIONS=0; index.html's reveal-game hook depends on Module.print).
- PERF-1: per-draw emscripten_get_now bind timer removed (PERFPROF createBGMs/bindLoopMs now 0).
- BGCACHE-2 + UBPOOL-1: UB/transient WGPUBuffers recycled via per-size FIFO pool (reusable after RHIEndFrame
  submit) -> stable handles; bind groups cached by exact entries; DawnLiveHandleRemove evicts groups using a
  dying handle. BGCACHE-3: reverse index rebuilt every 30 frames (per-drop RemoveSwap was O(n^2), 72%!).
- WALKCACHE-1/2: whole resolve walk skipped when inputs repeat (PSO serial, pending entries, UB content keys =
  buffer handle + ResTable resource ptrs, LM assoc version); hit re-binds cached BGs if all still present.
- UBNAMECACHE-1: derived UB shader name cached per UB.
- Profiling: node cdp_wprof.mjs <tab> <secs> [focusFn]  (per-thread V8 profiles incl. inclusive time).
  If threads stop answering, reload the page (a killed profile run can wedge worker inspectors).
- Open: rocket impact FX, distant trees move with camera, end-of-match crash, walk-key build cost
  (FlushPendingBindGroups still ~31%), game tick 23% (skeletal anim 9%).

## SESSION 2026-10-12 (evening) — fixes + public deploy
- SPRITELOC-1: Cascade sprite VS inputs remapped (DynamicParameter A5 declared before TexCoord A4).
- LOCALLOC-1: LocalVF (static mesh/SpeedTree) inputs remapped by declaration order: lightmap UV (A15) ->
  slot 4+nTC (was getting a duplicate texcoord -> blocky/wrong lightmaps). nTC = maxLoc-4 if the VS binds
  PrecomputedLightingBuffer.
- LWCFOLD-2: Primitive fold also fixes RelativeWorldToLocal/Previous inverse translation and folds
  Object/ActorWorldPosition High->Low; View folds every <X>High/<X>Low pair (5). Aimed at trees moving
  with camera (SpeedTree LOD distance) — user still reports moving trees: NOT fixed yet.
- DEPTHLOAD-1: RHIBeginRenderPass honored depth load action (was ALWAYS Clear -> DecalsAndTranslucency
  pass had wiped depth -> bullets/effects/turquoise medallions visible through walls).
- DEPTHDUMMY-1 (black scene-depth placeholder) tried then DISABLED: made translucent fog sheets opaque
  (dark world). Proper fix = bind real depth for translucency (read-only depth attachment + sampling).
- Diagnostics added: INSTDIAG/LOCALDIAG (VS inputs vs decl), WNDR MATDRAW (first submit of each material
  in MeshPassProcessor.cpp SubmitDrawBegin — Renderer module), BGINCOMPLETE/BGSKIPDRAW now printf.
- index.html: double-click = element fullscreen; 'stat fps' sent 4s after reveal; skinident URL flag.
- Open: Link plasma (M_LinkProjectile_Coma IS submitted, likely invisible due to depth-fade with grey
  depth placeholder), rocket explosions (same), trees, "brick" blocks at character feet (not decals),
  wall textures still off, end-of-match crash.

### Public deploy: https://ut4-hub.pick.haus (yieldllc/gitops clusters/homelab/websites/ut4-hub.pick.haus)
- nginx-unprivileged + 20Gi ceph-block PVC (websites ns), COOP/COEP/CORP headers, .data cached 7d,
  wasm/js no-cache. Memory limit 2Gi (256Mi OOMKilled during uploads).
- Tunnel: remote config PUT (version 26) + cloudflared configmap entry (dns-sync made the CNAME).
- Updating a build: kubectl access from the Mac is blocked by macOS local-network privacy -> run kubectl on
  nixtop (KUBECONFIG=~/.kube/pickhaus.kubeconfig). Upload = temp LAN server on the Mac
  (python3 -m http.server 8099 --bind 192.168.69.20 in ut-serve) + /tmp/hubfetch.sh inside the pod
  (wget per file to .part, size-checked, mv). Big tar-over-kubectl-exec streams time out.
  For a code-only update re-fetch UnrealTournamentwasm64.wasm, UnrealTournament.js, index.html
  (index.hub.html = index.html with the 7.6 GB note).

## SESSION 2026-10-12 (night) — FX depth, mesh particles, feet blocks, menu
- Trees: user confirmed FIXED (LWCFOLD-2).
- DEPTHCOPY-1 (fix-dawn-depthcopy.py): before any color pass that LOADS depth (DecalsAndTranslucency) the
  depth buffer (Depth24Plus, now created with TextureBinding) is blitted into an R32F/R16F copy; every 2D float
  "*SceneDepthTexture" slot binds the copy. Explosions/fireballs had SceneDepthTexture=DUMMY (grey) -> depth
  fade = fully transparent. Copy view is in the WALKCACHE key.
- MESHPARTLOC-1 (fix-dawn-meshpartloc.py): Cascade MESH particle VS inputs remapped (Link projectile Core/Coma,
  muzzle waves). T1 = first vec4<f32> input >= loc 4; rest follow decl order (stripped inputs keep numbers);
  dyn param present if loc >= T1+7 or decl A13 stride > 0. Link Gun now draws plasma (user to confirm look).
- NOFALLBACK-1 (fix-nofallback.py, Renderer: MobileBasePass/DepthRendering/CustomDepthRendering): materials
  whose cooked shader map fails to load are SKIPPED instead of drawn with WorldGridMaterial. That was the
  "bricks/boots" at characters' feet (M_Robust_BlobShadow[Inverse]). Also hides ShieldBelt overlay,
  M_Chill_StainedGlass*, MI_Chill_Icecicle_Cluster (all were grid before). Real fix = re-cook those materials.
- LMPAIR-1: LightmapResourceCluster_* resolved via THIS draw's PrecomputedLightingBuffer association first
  (100% hit). Did NOT change the patchy lighting (A/B identical) — kept as harmless.
- Patchy/blocky world lighting: confirmed lightmap-texture caused (Module._wndr_dbg(1) = grey lightmaps ->
  smooth). Lightmaps are full-res (LQ_Lightmap 2048^2 BC1, 12 mips) — not streaming. Still open: suspect
  lightmap UV/ScaleBias or LQ decode. Runtime toggles: _wndr_dbg bit0 grey LM tex, bit1 ambient PLB,
  bit2 old LM pairing.
- Diagnostics in: FXDIAG-1/2 (GWndrDiagDraw RHI_API flag set via EnqueueLambda in SubmitDrawEnd for Link
  materials; dumps bind groups once per PSO), LMSIZE-1, WndrViewInfo map (view -> name/fmt/size/mips).
- index.html/index.hub.html: ?menu=1 boots /Game/RestrictedAssets/Maps/UT-Entry (main menu); hub defaults to
  the menu (?map=chill for direct DM-Chill). printErr now also reveals on "Engine is initialized".
- Hub updated (code-only: wasm/js/index via /tmp/hubcode.sh pattern, LAN server 192.168.69.20:8099).
- Build box helper: /mnt/vms/ss-build/kick.sh restarts the wasm build (never pkill -f with a pattern that
  appears in your own ssh command line — it kills the ssh session). Wait for the rebuild script to EXIT
  (pgrep -f "bash [r]ebuild_wasm3.sh") before scp'ing the wasm, else you copy a truncated file.

## Repos + package CDN (2026-10-12 late)
- Code: github.com/ut4-hub/ut4-webgpu (PRIVATE). engine/ overlay (DawnRHI, SimplyStream src, modified
  engine/UT files), build/, tools/, web/, deploy/, docs/. Release build-2026-10-12 = all hub files.
- Packs: github.com/ut4-hub/ut4-webgpu-packages (PUBLIC), release packs-2026-10-12 = 12 .data files.
- Worker ut4-packs (custom domain ut4-packs.pick.haus, also ut4-packs.lucaspick.workers.dev) proxies
  /packs-<tag>/<name>.data -> release asset, manual redirect follow (redirect:'follow' gave CF error 1042),
  adds ACAO * + CORP cross-origin. Deploy: cd ~/workspace/ut4-webgpu-packages/worker && npx wrangler deploy
  (wrangler OAuth login done 2026-10-12; the tunnel cert token can NOT manage Workers).
- Pages: window.__wndrPkgBase -> locateFile sends *.data to the Worker. Hub default = Worker
  (?localpkgs=1 = homelab); local index.html default = local (?pkgs=cdn = Worker). Content change ->
  new packs-<date> release + bump the tag in both pages.
