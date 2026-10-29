# Native Gx render-target ownership

This M3 work package replaces the shader probe's inline, fixed framebuffer with
a reusable native Gx target owner. The full native client gate remains in place.
It does not expose legacy `CGxTex`/`CGxDevice` objects or complete their state,
projection, resource-cache or async contracts. Original fonts/GlueXML/login,
networking and an observed GrimGuzzler world session remain required for M3.

The integrated FFX continuation tracked in [#116](https://github.com/RovxBot/wowdecomp/issues/116)
adds `GlRenderTargets::Copy`: an ordered copy from the current color read buffer
into existing uncompressed color storage. Source and destination rectangles use
OpenGL bottom-left coordinates without a Y flip or scaling. Complete extents,
mip/plane, owner/current-context and attachment feedback are checked before the
write; texture binding is restored on success and GL failure. Ordinary sampler
references may remain bound during this transfer, as with `Upload`; no draw uses
the destination while it is written. Depth/compressed copies remain unavailable.
The legacy adapter admits the reached idle FFX render textures only; ordinary
callback-texture copies and `DeviceCopyTex`/stretch remain unavailable. The copy
uses preallocated storage without invoking a CPU source callback, matching the
original OpenGL `TexCopy` path. Copy qualification does not qualify subsequent
multi-input FFX shaders or complete original login rendering in #96.

## Source and caller audit

The reference is product 12340 at
`6023d6a1901637f58507c4eac987e1e450cdaa53`.
No 15595 or other fork implementation is imported. New code has no original
address or binary-verification annotations.

| Existing boundary | Adaptation and remaining contract |
| --- | --- |
| `CGxDevice::DeviceSetRenderTarget` | Native attachment pairs retain their storage and publish together. Color/depth size mismatches and wrong formats fail before changing GL. Adapting the legacy per-slot API and `m_textureTarget` cache remains device integration work. |
| `CGxDeviceOpenGl::IDeviceSetRenderTargetFBO` | An owner manages FBOs, immutable texture allocations and optional per-color depth renderbuffers. Explicit depth overrides implicit depth; removing it restores implicit depth. Replacing color removes stale depth. Failed FBO completion restores the old framebuffer and viewport. |
| `s_glTexTarget` / `s_glCubeFaces` | Reuse the existing `EGxTexTarget` enum. Support 2D, non-power-of-two, rectangle and all six cube planes in the original +X/-X/+Y/-Y/+Z/-Z order. Color RGBA8 and depth24 are implemented; other Gx formats and mip/resolve/copy operations remain unavailable through this API. |
| `DeviceSetCurWindow`, viewport and projection | Target binding sets a full, pixel-sized viewport. Returning to the drawable queries current SDL pixel dimensions and restores back-buffer reads/writes. The full device must still coordinate normalized viewport/scissor and projection constants; this owner does not silently modify shaders or legacy matrices. |
| `TexDestroy`, `m_apiSpecificData` / `m_apiSpecificData2` | Native handles use retained opaque storage instead of pointer-sized GL-name casts. Attachment/sampler references keep texture and implicit-depth storage alive until detached/unbound. The context outlives all handles. This does not replace the legacy cache/refcount ABI. |
| `CM2Shared::Load`, `CancelAllDeferredSequences`, destructor and `CM2Model::WaitForLoad` | No native ticket is cast to `CAsyncObject`. The shared-model destructor's cancellation return decides whether near/aligned buffer cleanup must be deferred. Sequence loads retain their own near buffers and callback/list ownership. Native file tickets and inspection-model arenas do not implement these caller contracts. Their adaptation is still required before full model rendering. |

`GlRenderTargets` owns framebuffer/sampler binding changes on its current context's
main thread. Resources from another owner are rejected. Sampled attachments are
rejected in either direction, conservatively including different cube faces.
Attachment transitions inspect actual GL texture bindings as well as native
handles. External code must not redefine native texture images or change
framebuffer/sampler bindings behind this owner. Raw GL draw/read operations remain
possible within the current target. Foreign read/draw framebuffer bindings fail
visibly rather than silently corrupting the owner's cache.

Creation validates GPU size limits before allocation, disables pixel-unpack-buffer
interpretation for null image storage and restores texture/renderbuffer/PBO
bindings. Resize uses a new immutable allocation and a validated attachment
transition; existing attachments keep their previous allocation until replaced.
The owner does not resize a live texture behind another attachment. FBO changes
use a temporary framebuffer and check completeness before publishing its handles.
All GL operations and destruction require a live current context; invalid
destruction terminates, matching the existing native program/context contract.

## Validation

All compilation and C++ fixtures run in
[M3 OpenGL Actions](../../.github/workflows/m3-gl.yml), using the existing
`linux-gl` and `linux-gl-asan` presets. The generated graph allowlists the new
source and `GlRenderTargetsCheck` and continues to reject legacy Windows device
objects, ABI mixing and signature bypasses. The full Windows reference remains
a separate regression build.

`GlRenderTargetsCheck` exercises three complete context/owner cycles:

- RGBA8/implicit depth, explicit-depth override/removal, replacement with a new
  extent, depth-only FBOs, rectangle/non-power-of-two targets and pixel readback.
- Six independently cleared/read-back cube color and depth faces.
- Invalid dimensions/formats/planes, mismatched attachments, foreign-owner and
  foreign-thread rejection, retained attachment/sampler lifetimes and GL deletion.
- Feedback checks in both directions, active-unit restoration and allocation
  state preservation with an intentionally undersized pixel-unpack buffer bound.
- A deliberately redefined zero-width texture image forcing actual incomplete-FBO
  rollback. This injected misuse is distinct from ordinary production behavior.
- Drawable restoration, resize and logical-versus-pixel viewport checks.

The existing `NativeGlCheck` now uses the same owner for its synthetic and original
UI-shader FBO pixels and refreshes the drawable viewport during presentation.
Original-data mode keeps the original-key signaturefile and registered MD5 policy.
It displays the original loading-bar texture; this remains a shader/asset probe.
Hosted Xvfb/Mesa checks, actual GPU execution and physical operator observations
must be recorded separately. Historical context/shader evidence is not relabeled
as validation of this implementation. No new physical-input acceptance is implied.

Selected implementation `2eb6dd9ee61c15aa5b788a6f7856cdb2019e9b4c` in
[PR #20](https://github.com/RovxBot/wowdecomp/pull/20) passed all eight implementation
PR workflows, including full Windows x64 compilation. Both Linux GL configurations
passed 16 CTest checks. The [durable evidence](render-target-validation.json)
preserves the distinct Windows merge checkout, hosted manifests, ZIP/binary
verification and exact-source GPU reports. The PR merged as `4d74dd0c5330f92c3fc282329fc071c61b23f62a` on 2026-10-18.

On RX 7900 XTX / Mesa 26.2.5 / SDL X11 in the Wayland desktop, release and
ASan/UBSan/leak-enabled target checks each passed three context/owner cycles.
The integrated original-data probe passed one release and three sanitizer cycles,
with original signature authentication, five UI-shader FBO pixels, empty stderr
and zero native workers at each shutdown. No suppression/preload/audit shim was
used. The loading-bar capture was inspected; no original fonts/login UI, stereo
rendering, full model rendering, Windows runtime or native gameplay is established.
Local Python checks discovered 24 tests: 23 passed, one CMake selection check was
unavailable locally; all 24 ran in Actions. File references and whitespace passed.
All client/dependency compilation ran in Actions.

After downloading and hash-verifying a passing artifact, the existing
[runtime wrapper](../../Tools/Porting/run_native_gl.py) runs the integrated probe.
Add `--render-target-checks` to hash-verify and run `GlRenderTargetsCheck` before
the probe. Its separate result is retained in `render_target_checks`; a failure
or timeout in either program fails the combined command. The fixture reports its
actual GL vendor/renderer/version and requires no game data. Keep captures,
settings and reports external to the repository and both game installations.
