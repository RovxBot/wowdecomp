# Authored image mip chains and local readiness

This extends the qualified [file-image Services](IMAGES.md) and [legacy Gx
adapter](LEGACY_DEVICE.md). It adapts this product's 12340
`CGxDeviceOpenGl::ITexMarkAsUpdated` lock/latch/unlock traversal and
`Services/Texture.cpp` priority/readiness boundary. No donor body, dependency,
game data or new OFFSET/VERIFY claim is imported. The original Windows paths
remain selected by the full Windows client.

Native file-image handles now accept nearest-mip-nearest, linear-mip-nearest and
linear-mip-linear filtering in addition to nearest/linear. Mipped requests require
the complete authored BLP2 chain through 1x1, with both dimensions clamped to one
at each level. Missing levels fail with diagnostic status; no levels are generated.
Nonmip requests validate the supplied chain and retain only level zero. Each sampler
variant keeps its own cache identity. Opacity considers every retained level.

The native Gx adapter allocates the required immutable mip storage and calls Lock
once at the original extent, Latch in increasing level order at each level's extent,
then Unlock once at the original extent. A completed Lock receives exactly one
Unlock even when a lower Latch/upload fails. Failed updates stay dirty for a full
retry. Mipped dirty updates refresh the entire chain, matching the original OpenGL
consumer; single-level updates retain their qualified dirty-rectangle behavior.
Changing a sampler across mip/nonmip allocation classes fails visibly and requires
resource recreation. Mipped render targets, anisotropy, generated mips, base-mip
aliasing, forced tracking, cube and compressed callback uploads remain unavailable.

Creation option bit `0x10` selects original UI priority 129 instead of ordinary
image priority 130. An identical cached request with this bit promotes a queued
ticket while preserving its handle/refcount. It cannot preempt a current read.
Wait modes are 0 (query), 1 (bounded completion wait) and 2 (wait for local data).
Original mode 2 avoids waiting when `CAsyncObject::m_char24` marks remote streaming;
the native service only supports local readers, so modes 1 and 2 both wait up to
30 seconds. Neither mode exposes native tickets as legacy async objects.

The existing 32-handle/request, 32 MiB source and 2,097,152 base-pixel limits remain.
A shared 256 MiB retained-pixel budget now includes all levels and retired requests
until their pixels are released. Worker-side temporary read/decode allocations are
separately bounded by the source and per-image limits. Owner-thread GPU/cache
mutation, callback delivery, cancellation and shutdown ordering remain required.

`NativeImageCheck` uses independent authored level colors to check magnification,
each minification level, trilinear blending, lower-level alpha, incomplete-chain
failure, wait 2, UI priority and duplicate-request promotion. `LegacyGxCheck` checks
nonsquare mip extents, failure at a lower level, exactly-once Unlock, retry,
whole-chain dirty updates and incompatible sampler/target rejection. Both retain
their three-cycle hosted release/strict sanitizer checks. Optional original-data
mode adds unchanged `Tileset/Elwynn/ElwynnDirtBase.blp` with trilinear minification
through actual Services/Gx/UI shaders, alongside the original loading-bar image.
Original-key package authentication and selective registered-MD5 policy stay active.

All compilation and hosted checks run in [M3 GL Actions](../../.github/workflows/m3-gl.yml).

## Qualification

[PR #37](https://github.com/RovxBot/wowdecomp/pull/37) is qualified at
`41c0a78e7f25bf6a040a59560c5f33502297d6f9`, merged as
`0d90dcdd741e43418016aaeeb15a66c23be2f1f8`. The
[durable record](image-mip-validation.json) preserves all seven triggered passing
PR workflows, 23 CTest checks per Linux GL configuration, three verified archive
digests and all 35 packaged executable hashes/types. Full Windows x64 compiled
with signatures enabled at matching-tree checkout
`6c2521c2a897f9e4d192404b118139f784b7e22b`.

Exact-source RX 7900 XTX / Mesa 26.2.5 / SDL X11 release and strict ASan/UBSan/leak
binaries each passed three authored and three original-data image cycles, including
the unchanged 256x256 Elwynn image's authored mip chain with trilinear minification.
Original-data Gxu and Gx checks passed three cycles each; preview and all four
resource fixtures passed too. All eighteen executions exited 0 with empty stderr
and zero workers at probe shutdown. The original loading-bar capture hashes match
each other and the prior qualified capture; the release capture was inspected.
The terrain mip check uses FBO telemetry and has no new terrain/world capture.
Original-key authentication observed 28,948 records per original-data cycle;
registered MD5 checks remain enabled. Local source checks: 43 passed, one platform
skip. No client/dependency compiled locally. There were no failed implementation
fixture runs; prior [image failures/evidence](image-validation.json) stay intact.

Frame's `0x17` still requests disk lookup and atlas eligibility, which this native
surface rejects. Those flags must be adapted explicitly before selecting actual
Frame factories. Alternate TGA lookup, atlases, general async objects, actual
Frame/Lua/GlueXML/login rendering, transport and observed gameplay remain open.
This resource fixture does not qualify world rendering. M3 remains incomplete;
the external server revision discrepancy and interactive sign-in preference stay.

## Next original Frame callers

The subsequent [layout package](LAYOUT.md) now qualifies actual `CLayoutFrame.cpp`
and `CFramePoint.cpp`. Font/string/region/root factories and disk/atlas image
contracts remain next. The initial source audit below records the prerequisites
that layout work resolved, without replacing this image package's historical scope:

- `CLayoutFrame` preserves uninitialized bits of `m_layoutFlags` in its constructor;
  `CFramePoint` reads uninitialized `m_flags`, and its allocator uses MSVC-only
  `type_info::raw_name`. Native initialization/allocator corrections must preserve
  the historical Windows bodies and their provenance.
- Native layout entry points need owner-thread affinity, live-object/pool shutdown
  accounting, bounds on unsigned anchor indices and finite dimensions/scales.
  Anchor dependency destruction and deferred resize must execute actual original
  list/point consumers; XML parsing alone does not qualify them.
- `CLayoutFrame::LoadXML` needs original `StringToFramePoint` and
  `LoadXML_Dimensions` behavior from `LoadXML.cpp`, including original absolute
  coordinate conversion. Do not replace it with a new pixel-only layout system.

Then connect actual `CSimpleFont`, `CSimpleFontString`, `CSimpleRegion` and root
owners; retain font/text-block/cache lifetimes and SDL size invalidation. Adapt
`CSimpleTexture` disk/atlas/image-mode behavior explicitly. Finally connect
`FrameXML_ProcessFile`/`FrameXML_CreateFrames` factories, retained templates and
ordered Lua execution. Preserve the second digest comparison in `CGlueMgr::Resume`
before `FRAMES_LOADED`; only actual original GlueXML/login rendering qualifies
that UI gate.
