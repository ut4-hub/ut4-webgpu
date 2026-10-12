# Handoff: ES31 WebGPU bind-group validation failure (`GraphicsCreateBindGroup`)

**For:** an agent working in the `simplystream-platform` repo (WebGPU RHI).
**Branch:** check out **`6.0`** (the bug reproduces on the 6.0 cook lane; do
NOT work on 5.8 for this). The affected file exists on both lines.
**Filed by:** FutureLands/Anvil Worlds session, 2026-07-04.
**Priority:** mobile blocker — ES31 is the mobile feature level and a
first-class target; this fails cloud verify on ES31 for every cook.

---

## Symptom

Strata `verify es31` fails hard:

```
Assertion failed: BGWriter[i].ValidateEntriesForCreate(*BindGroupLayout, i, TEXT("GraphicsCreateBindGroup"))
[File: Source/Runtime/WebGPURHI/Private/WebGPUPipelineState.cpp] [Line: 934]
```

Reproduced on FutureLands cook **rev 1849** (engine `eac20c3f`, platform
submodule `60763411`). SM5 verify does **not** hit this assert (see "Scope"
below).

## What the assert actually means

`FWebGPUBindGroupWriter::ValidateEntriesForCreate`
(`WebGPUPipelineState.cpp:212`) rejects a bind group because at least one
**entry doesn't match the pipeline's bind-group layout** before
`wgpuDeviceCreateBindGroup`. An entry is "malformed" if any of:

- `ResourceCount != 1` — the entry has zero or >1 of {buffer, sampler,
  textureView} set.
- `!bHasBindingType` — `StageInfo->Types` has no type for this `binding`
  (or `EntryIndex`) → the shader's reflected layout and the RHI's entry
  list disagree on which slots exist.
- `!bExpectedResource` — the bound resource kind (buffer/sampler/texView)
  doesn't match the layout's `WgpuBindingType` for that binding.
- `!bStateAllowsCreate` — entry state is `DefaultResource` on a *buffer*
  binding (buffers must be explicitly bound, not defaulted).

**Crucial:** immediately before the assert, the same function emits a
detailed `UE_LOG(LogWebGPURHI, Error, ...)` naming the exact bad entry:

```
WebGPU malformed bind-group entry before GraphicsCreateBindGroup:
  set=<N> entry=<N> binding=<N> type=<...> state=<...> hasType=<0|1>
  resourceCount=<N> name=<...> buffer=0x.. sampler=0x.. textureView=0x.. offset=.. size=..
```

**Step 1 of the fix is to capture that line** — it names the shader stage,
the binding slot, the expected type, and what was actually bound. Grep the
raw verify/doctor console for `malformed bind-group entry`. Do NOT rely on
Strata's `verify.json` digest — it drops WGPU pipeline-validation detail
(see the engine-plugin CLAUDE.md `fatals:0 ≠ no errors` note); capture the
**raw** console.

## Scope — this is ES31-specific and pre-existing (NOT a recent regression)

Evidence gathered while triaging from the FutureLands side:

1. **Not caused by the recent engine/platform changes.**
   `WebGPUPipelineState.cpp` is **byte-identical** between the previously
   deployed rev (1845, platform `0a594a6e`) and 1849 (platform `60763411`).
   The only platform delta between those revs is `SimplyStreamToolChain.cs`
   — an FFT-bloom-kernel **cold-build read-with-retry** (UBT), which touches
   nothing shader- or WebGPU-related. The engine-side deltas were all
   AngelScript binding changes, which can't affect WebGPU pipeline creation.
2. **SM5 does not hit it.** SM5 verify boots and renders (its verify
   "failure" is a *different, unrelated* issue — the harness misreads Anvil
   Worlds' intentionally-black-space default scene as "canvas black"; that's
   a verify-harness gap being fixed on the FutureLands side, not an RHI bug).
   So the malformed bind group is specific to the **ES31 feature-level
   pipeline**, which SM5 doesn't exercise.
3. **It was simply never surfaced before** — every prior FutureLands probe
   ran SM5 on desktop (WebGPU auto-selects SM5 on a BC-capable GPU). Cloud
   verify exercised ES31 for the first time and caught a latent bug that has
   been present at least since platform `0a594a6e` (deployed rev 1845 has it
   too — it's an all-revs mobile bug, not 1849-specific).

Conclusion: an **ES31 shader's reflected bind-group layout diverges from
what the RHI assembles** for that pipeline. The likely area is the ES31 vs
SM5 differences in GPUScene / bindless / uniform-buffer binding config.

## Suggested investigation path

1. **Reproduce + capture the log line** (above). The `set`/`binding`/`type`/
   `name` fields point straight at the offending shader + slot.
2. From `name`/`set`, identify the **shader** and its bind-group layout.
   Compare the ES31 vs SM5 reflected layout for that pipeline — which slot is
   present in the layout but unbound (or bound with the wrong kind) at ES31.
3. Check ES31-specific binding config: GPUScene ES3_1/SM5 config, device
   limits/tier, bindless-vs-bound differences. Deep-dive doc:
   **`Upgrade/webgpu_rendering.md`** (GPUScene ES3_1/SM5 config, device
   limits/tier, codegen fixes) and **`track_bugs.txt`** for live state.
4. **Rule out a DDC/shader-staleness artifact first (cheap):** bump
   `UE_SHADER_WEBGPU_ES31_VER` in
   `Source/Developer/WebGPUShaderFormat/Private/WebGPUShaderFormat.cpp`
   (replace the GUID) and recook ES31 clean. If the malformation disappears,
   it was stale ES31 shader reflection, not a code bug. If it persists, the
   RHI's ES31 bind-group assembly (or the ES31 shader codegen) has the real
   defect.
5. If codegen: the WebGPU cooker/compiler is
   `Source/Developer/WebGPUShaderFormat/` — after any change, bump
   `UE_SHADER_WEBGPU_ES31_VER` and verify SM5 **and** ES31.

## Repro / verify-a-fix

- Cook + verify via Strata (operator flow). FutureLands is registered as
  repo `futurelands`, `engine: 6.0`. A cook of its head + `verify es31` on
  the real-GPU pool reproduces this.
- Cook + verify can also be pointed at any SimplyStream project on the 6.0
  lane that exercises ES31.
- `strata pipeline futurelands` shows the verify verdict; capture the raw
  ES31 verify console for the `malformed bind-group entry` line.
- A fix is proven when **ES31 verify passes** (bind group created, scene
  renders) on the real-GPU pool — not just `verify.json fatals:0`.

## Reference anchors (paths in this repo, 6.0)

- Assert site: `Source/Runtime/WebGPURHI/Private/WebGPUPipelineState.cpp:933`
  (graphics) / `:742` (compute has the same guard).
- Validator: `FWebGPUBindGroupWriter::ValidateEntriesForCreate`,
  `WebGPUPipelineState.cpp:212` (the malformed-entry conditions + the
  diagnostic log).
- WebGPU cooker/compiler + shader GUIDs:
  `Source/Developer/WebGPUShaderFormat/`.
- Rendering deep-dive: `Upgrade/webgpu_rendering.md`; live bug/deploy state:
  `track_bugs.txt`.
