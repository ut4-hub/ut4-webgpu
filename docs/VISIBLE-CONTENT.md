# UT4 WebGPU in-browser — local Mac run + visible-content assessment (2026-10-10)

## Setup that now works (Mac-only, no nixtop dependency)
- Game bundle copied from `nixtop:/mnt/vms/ss-build/ut-serve/` →
  **`/Volumes/ExFAT 1TB/ut4-webgpu/ut-serve/`** (6.7 GB, symlinks dereferenced)
  - wasm md5 verified: `8f157c3c80090af4ed18bd469cb86a8b` (best-known build)
- Local server: `python3 serve_local.py 8091` (COOP/COEP + range support)
- Runner: `node cdp_logger.mjs "/Applications/Brave Browser.app/..." http://127.0.0.1:8091/ 9333 > ut-boot.log 2>&1`
- Screenshots: `node /tmp/ut-screenshot.mjs <out.png>` (CDP Page.captureScreenshot)

## Boot works end-to-end on the Mac
WebGPU device acquired → wasm instantiates (16s) → engine init (~15s) →
DM-Chill loads, match InProgress, 1 human + 5 bots, live gameplay renders.

## What the map looks like (screenshots in this dir)
- shot-3/4/5: **DM-Chill geometry renders — walls, ceiling with snow, beams,
  icy stalactites with correct color, pickups visible+bobbing, bots present**
- shot-6/7: red damage/death-cam flash over the same visible scene
- shot-1/2 (first boot): frozen red/black death cam after the tick froze

## Answer to "does the map have fully cooked content we can see?"
**Geometry: yes. Lighting: NO — everything renders flat/unlit (near-black).**
Root cause chain (verified from boot log):
1. `WNDR WALLLIGHT DUMMY-UB name=PrecomputedLightingBuffer (world lighting UB is zero)`
2. `WNDR WALLLIGHT DUMMY-TEX name=LightmapResourceCluster_LightMapTexture (baked lightmap/skylight is dummy)`
3. Same for `LightmapResourceCluster_StaticShadowTexture`
- The **DM-Chill_BuiltData IS cooked and packaged** (lightmaps baked Aug 20;
  TOC in ContentBootstrap.data pkg0, payload inside pkg4) — so it's NOT a
  packaging gap. The runtime binds DUMMY lightmap resources anyway →
  the IoStore/IAS-loaded BuiltData isn't reaching the renderer (load-order or
  lightmap-cluster lookup failure in the wasm path).
- Only 7 unique materials fail shader compile (blood splash, blob shadows,
  UDamage overlay/glass, GPU splash — all effects, cosmetically minor).
- 16 `.ufont` reads fail (scoreboard/chat fonts) → HUD text may be affected.

## Remaining runtime blocker (unchanged from Aug)
**Game/render thread freeze ~7s of game-time in (frame ~36-49), every boot.**
`WndrHeartbeat: GAME THREAD STALLED ~15s … presentedFrame=-1` — the known WebGPU
proxy-thread livelock (`webgpu_proxy.h` busy-spin fix was WRITTEN in Aug but
BUILD-BLOCKED: no compilable WebGPURHI source on the box; precompiled .o only).
After the freeze the last frame stays on screen (usually the death cam, since
the player dies in the bot crossfire right around then — no fire input to respawn).

## Next steps (suggested order)
1. **Fix the lightmap dummies** — highest visual payoff. Instrument the
   BuiltData package load (does `LoadPackage(DM-Chill_BuiltData)` succeed? does
   the lightmap cluster resolve?) — likely an IAS/PathPak resolver miss for
   `_BuiltData` maps loaded via the map's package.
2. **Revive the proxy-livelock fix** — author a minimal .cpp compiling
   webgpu_proxy.h into the wasm build (option (a) from Aug), or get WI's stub.
3. Fonts `.ufont` reads fail — check packaging of UI/Fonts dir.
