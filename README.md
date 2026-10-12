# UT4 WebGPU

Unreal Tournament (UE 5.8, SimplyStream platform) compiled to wasm64 and rendered with WebGPU
through a clean-room `DawnRHI`. Runs DM-Chill and the main menu in the browser.

Live: https://ut4-hub.pick.haus (main menu by default, `?map=chill` goes straight into DM-Chill).

## Layout

| Path | What |
|---|---|
| `engine/Engine/Source/Runtime/DawnRHI` | The WebGPU RHI (Dawn / emdawnwebgpu). |
| `engine/Engine/Platforms/SimplyStream` | Platform source (prebuilt third-party libraries and binaries left out, see `engine/LARGE_FILES_NOT_INCLUDED.txt`). |
| `engine/Engine/...`, `engine/UnrealTournament/...` | Engine and game files modified for this port (overlay onto a UE 5.8 tree). |
| `build/` | Build-box scripts: `rebuild_wasm3.sh` (wasm build + JS post-link patches), `kick.sh`, cook/package/iostore scripts, all `fix-*.py` source patches. |
| `tools/` | Mac-side helpers: CDP drivers (`cdp_*.mjs`), patch scripts (`fix-*.py`, idempotent, marker-guarded). |
| `web/` | `index.html` (local), `index.hub.html` (deployed as `index.html` on the hub), local servers. |
| `deploy/` | Kubernetes manifests for ut4-hub.pick.haus (canonical copy lives in yieldllc/gitops). |
| `docs/` | Session handoffs and investigation notes. Start with `docs/HANDOFF-NEXT-SESSION.md`. |

## Built files

The runnable build (wasm, JS loader, ~7.4 GB of content `.data` packs) is attached to the GitHub
Releases of this repo, not committed. Serve those files plus `web/index.hub.html` (as `index.html`) with
`Cross-Origin-Opener-Policy: same-origin`, `Cross-Origin-Embedder-Policy: require-corp` and
`Cross-Origin-Resource-Policy: same-origin`; `deploy/` has a working nginx config.

## Build loop (summary)

1. Apply/edit patches on the build box (`/mnt/vms/ss-build`), each `fix-*.py` is idempotent.
2. `bash build/kick.sh` (or `rebuild_wasm3.sh`), wait for the script to exit (not just "BUILD EXITED").
3. Copy `UnrealTournamentwasm64.wasm` + `UnrealTournament.js` to the serve dir, reload.

Contains Unreal Engine source; Epic's EULA applies.
