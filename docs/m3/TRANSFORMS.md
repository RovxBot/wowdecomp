# Native Gx transforms and normalized raster state

This M3 work package connects legacy transform contracts to the native context
and retained target owner. It reuses unchanged scalar `CGxMatrixStack`,
`C44Matrix` and `C33Matrix` sources in the native ABI allowlist. The native probe
now supplies original UI shader constants through this boundary, including a
scaled/translated FBO draw, instead of supplying identity constants directly.
The full native client remains gated; M3 is not complete.

## Source and caller audit

The reference is product 12340 at
`840dc0fbda9206573a167a55f9051d3d6e3bfd38`, including the qualified render-target
work in [PR #20](https://github.com/RovxBot/wowdecomp/pull/20). This work is stacked
on that still-open PR. No 15595/fork implementation, new asset, original address
or binary-verification annotation is imported. Existing scalar source annotations
retain their historical provenance; hosted execution is not binary revalidation.

| Boundary | Native adaptation and limits |
| --- | --- |
| `CGxMatrixStack`, `GxXformSet/Push/Pop/Translate/Scale` | Reuse four-entry stacks and Tempest row-vector math, including level-three saturation/copy and base-level pop behavior. Accept unsigned stack indices and validate before enum interpretation; validate finite results before publishing mutations. Device projection remains separate from the projection stack slot, as in the original callers. |
| `GxXformSetViewport`, OpenGL viewport/scissor synchronization | Clamp normalized edges, retain ordered nonempty viewports and ordered [0,1] depth ranges, and round pixel edges with +0.5. Use actual bound attachment/drawable pixel dimensions, including depth-only FBOs. Scissor intersects the normalized target extent and allows empty results; invalid/nonfinite input fails before changing state. |
| `IXformSetProjection`, `IXformGLModelView` | Load the original Z row/column adaptations and world-times-view order into compatibility GL. Offscreen targets flip projection Y and reverse the cull selection; drawable restoration removes that flip. Retain the caller's GL matrix mode. Nonidentity texture stacks fail visibly at synchronization until texture/texgen GPU delivery is integrated; clip planes, stereo and resource/state-cache integration remain separate work. |
| Perspective projection branch | Preserve the inspected literal bit-31 test and `d3 != 1` perspective *0.2 branch. The named `NormalProjection` enum is bit seven; it does not substitute for bit 31. This records source behavior and does not diagnose or fix the historical warped Windows OpenGL scenes. |
| `GxXformViewProjNativeTranspose`, Gxu UI shader constants | Build view-times-native-projection, undo its Z row conversion and transpose into four env float4s. The original helper excludes world; fixed-function world/view and UI constants remain distinct. The original `0x10003` UI probe now consumes these values on every draw and checks offscreen orientation plus a nonidentity view. |
| `GlRenderTargets::Bind/View` | Call `Sync` after a successful target change and before a draw. It refreshes normalized viewport/scissor and projection against the actual target. Target validation/rollback remains owned by the retained target owner. This adapter owns its raster/matrix state but is not the complete legacy device cache. |

All access requires the context's main thread with that context current. The
context and target owner outlive the adapter. Scalar state setters validate before
publication; `Sync` checks combined matrices before touching GL and reapplies its
state without assuming a legacy dirty cache. UI constants require synchronization.
Unsupported full-device behavior is not exported as fake-success methods.

## Validation and operator use

All C++ and dependency compilation runs in
[M3 OpenGL Actions](../../.github/workflows/m3-gl.yml), through the existing
`linux-gl` and `linux-gl-asan` presets. `GlTransformsCheck` runs three complete
context/target/adapter cycles. It checks all eleven stacks, order/saturation,
invalid inputs, normalized pixel edges, depth range, matrix-mode restoration,
perspective branch, world exclusion from UI constants, fixed-function and ARB
transformed pixels, offscreen/scissor/depth/cull behavior, depth-only targets,
resize and foreign-thread rejection. Hosted Xvfb/Mesa rendering is separate from
actual GPU execution and operator input acceptance.

The native graph allowlists only the three reused scalar sources plus the new
adapter and fixture; WGL/D3D/legacy device objects and ABI mixing remain rejected.
The Windows full-client build remains a separate regression reference. The
hash-checking [runtime wrapper](../../Tools/Porting/run_native_gl.py) adds
`--transform-checks`; every requested fixture binary is checked before any program
executes. Target, transform and probe outcomes are recorded separately, and any
failure/timeout fails the combined command.

After downloading a passing Actions artifact outside the repository:

```sh
python3 Tools/Porting/run_native_gl.py \
  --artifact /tmp/wowdecomp-m3-transforms-release \
  --render-target-checks --transform-checks \
  --data /mnt/games/3.3.5aClean/Data \
  --settings /tmp/wowdecomp-m3-transforms-settings \
  --cache /tmp/wowdecomp-m3-transforms-cache \
  --frames 30 --timeout 120 \
  --snapshot /tmp/wowdecomp-m3-transforms.ppm \
  --output /tmp/wowdecomp-m3-transforms-release.json
```

Use the sanitizer artifact with `--sanitizers --cycles 3`, separate writable roots
and a separate report. No local compilation is permitted. Original-key RSA and
registered MD5 policy remain enabled. Original UI shader execution/loading-bar
output is distinct from original fonts, GlueXML/login, complete models/world,
portable networking, authentication and observed GrimGuzzler gameplay. Prior
operator observations keep their historical source identities. No new physical
input acceptance is implied. The external server snapshot's tag/embedded-revision
difference remains recorded in [server identity](SERVER_IDENTITY.md).

Selected implementation `a1c43f820448cbb0b0c0eb2fda2ab59216687415` in
[PR #21](https://github.com/RovxBot/wowdecomp/pull/21) is qualified and merged as `37bc74b492ce3f547d8b222676a7cc547e7727ab` on 2026-10-18.
The [durable validation](transform-validation.json) records all eight successful
implementation PR workflows, 17 CTest passes in each Linux GL configuration and
full Windows x64 compilation at separate PR merge checkout
`b18ed2af2622a8dd15e67a5863a9997b1f143f13`. Downloaded ZIPs, all packaged binaries,
notices and Windows AMD64 PE identities matched their manifests. All 26 Python
source/launcher checks ran in the hosted Linux job. Local checks discovered 26:
25 passed, one CMake selection fixture unavailable locally; file references,
JSON and whitespace checks passed. All client/dependency compilation was hosted.

Exact-source RX 7900 XTX / Mesa 26.2.5 / SDL X11 on Xwayland release and
ASan/UBSan/leak-enabled target and transform fixtures each passed three cycles.
The original-data probe passed one release and three sanitizer cycles, including
both the original shader pixels and scaled/translated pixels. Every executable
had empty stderr; each original-data probe shutdown reported zero native workers.
No suppression, preload or audit shim was used. The 960x640 original loading-bar
capture was inspected. Original-key authentication covered 28,948 signaturefile
records; both UI shader members still lack registered MD5 entries under the
unchanged selective policy. Stereo permutation upload is separate from rendering
permutation zero. No new physical-input, original font/login UI, Windows gameplay,
macOS graphics or native server-session acceptance is recorded.

The initial release at `0373a4cf7c31e77cbd4ff89e907513d465d01edb` passed
17 CTest checks. Its sanitizer run caught an invalid-enum load in the new negative
input fixture: passing -1 as the unfixed `EGxXform` enum is itself undefined before
range validation. The native boundary now accepts unsigned indices, preserving
normal enum callers while rejecting raw invalid values before interpretation.
Final hosted/sanitizer and actual-GPU qualification remain pending; the evidence
record will retain the failed run and final source/artifact identities.

The initial actual-GPU probe found a second integration gap: Radeon returned
matrix mode zero before any compatibility matrix had been selected. Restoring
that value failed with `GL_INVALID_ENUM`; debugger argument tracing identified
the restore call. The standalone fixture had explicitly selected `GL_TEXTURE`,
masking this missing startup step. Adapter construction now selects the legacy
`IStateSetContextDefaults` `GL_MODELVIEW` baseline, and the fixture synchronizes a
fresh context before making any matrix-mode selection. Subsequent sync preserves
the caller's mode. Native offscreen orientation uses the original default
capability branch (`unk_00B0 == 0`); the alternate capability is not exposed.
These failures remain diagnostic history, not qualified runtime passes.
