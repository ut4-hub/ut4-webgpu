# WebGPU visible-content verification — 2026-10-10 (Mac local run)

## Verified: the cook is COMPLETE and correct — the runtime drops the lightmaps

### What's packaged (verified on nixtop, `ut-iostore/`)
- `DM-Chill.umap` + `DM-Chill_BuiltData` (uasset + 5 .ubulk, ~75 MB) **are in container
  utcontent4** (TOC directory index has 6 entries for them; ubulk 001–005 + uasset).
  (`.uexp` doesn't appear in the TOC by name — normal for IoStore: uexp merges into
  the package's main chunk.)
- All 8 containers mount at boot (`LogFilePackageStore: Mounting container …
  NumPackages=6634/1589/1184/1554/276/3478/1981` ≈ 16.7k packages).
- Only 7 unique materials fail shader compile (blood splash, blob shadows ×2,
  UDamage overlay ×2, UDamage glass, GPU water splash) — cosmetic effects only.
  **Walls/floors/props compile fine** — that's why geometry renders.

### What the runtime does with the lightmaps (the actual bug)
1. Map loads; the render hook logs `WALLLIGHT DUMMY-UB PrecomputedLightingBuffer
   (world lighting UB is zero)` + `DUMMY-TEX LightmapResourceCluster_LightMapTexture`
   → every BSP/prop primitive draws **unlit/flat-black** (the near-black look).
2. At the SAME moment: **800×** `LoadErrors: While trying to load package None,
   a dependent package None (<64-bit-hex>) was not available` +
   `FPackageName: Unable to identify a valid mount point … package root is unknown`.
   → the lightmap cluster's texture references resolve to **GUID-only packages the
   loader can't map to a name/mount**.
3. Root enabler: **`LogAssetRegistry: Failed to load premade asset registry.
   LoadResult == 1`** (= `NotFound` per `ARLoader.h` ELoadResult enum). The
   AssetRegistry.bin **IS preloaded** into MEMFS at `/UnrealTournament/AssetRegistry.bin`
   (bootstrap pkg0, 8 MB, verified in ContentBootstrap.js manifest), but
   `FPreloader::TrySetPath()` → `IFileManager::FileExists("../../../UnrealTournament/
   AssetRegistry.bin")` returns false at that point in boot.
   - The launcher's own `stat()` of the same relative path **succeeds** (log line
     `cwd=/ relstat=0`), so the file and path are fine — the failure is in the
     platform-file chain used by IFileManager at that boot phase (the IAS/PathPak
     wrapper vs the forced-physical POSIX file, ordering issue around
     `IoStore mount: forcing physical platform file` at 06.722 vs AR load at 06.762).
4. Test performed: regenerated bootstrap with **absolute MEMFS paths**
   (`@/UnrealTournament/AssetRegistry.bin` etc.) → boot crashed with
   `memory access out of bounds` right after Running (launcher expects the
   original path layout). Reverted.

### What "working for WebGPU" means right now
- Boot ✅, menu ✅, DM-Chill in-match with 5 bots ✅ (~27 FPS while alive)
- Geometry/props/weapons visuals: partial — untextured/flat but the right shapes
- **Baked lighting: loaded-but-dropped** → everything near-black
- Known separate blocker unchanged: game/render thread freeze ~7 s of game-time in
  (WebGPU proxy livelock, fix written in Aug but build-blocked)

## Fix candidates for the lightmap drops (in order)
1. **Make the premade AssetRegistry load**: the AR is what maps GUID-only package
   refs → names/mount points. Debug why `IFileManager::FileExists` misses the
   MEMFS file at that phase (likely the IAS PathPak platform-file wrapper still
   being the active low-level file for `FileExists` even after the "force" — the
   force changes `GetPlatformFile()` but the IAS dispatcher may own
   `IAsyncReadFileHandle`s). A launcher-side workaround: also mount
   `AssetRegistry.bin` under every alias UE might query, or delay the AR load
   until after the IoStore mount.
2. **Patch the WALLLIGHT path**: if the AR can't be fixed quickly, patch the
   lightmap cluster resolve in DawnRHI/Launcher to fall back to a synthetic
   ambient (bind a 1×1 neutral lightmap + flat sky) so maps aren't black while
   the GUID resolution is fixed.
3. Longer term: re-cook with `-NoStreamableLightmapPackages`-style settings or
   patch the cooker so lightmap textures live in the map package (no GUID refs).
