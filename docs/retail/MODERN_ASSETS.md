# Native modern-model workstream

Scope and evidence live in [#140](https://github.com/RovxBot/wowdecomp/issues/140),
with the Alpha 0.2.0 feature baseline in [#211](https://github.com/RovxBot/wowdecomp/issues/211).
The bounded next character prototype is [#214](https://github.com/RovxBot/wowdecomp/issues/214).
This is a source compatibility inventory, not a claim that a modern model has
been displayed or that a particular modern asset pack is supported. No model,
texture, skeleton, animation, DB2 or retail asset reader is added by the Edit
Mode prerequisite.

## References and reusable boundaries

Full revisions and licence decisions are in the
[reference lock](../../Tools/LinuxClient/RetailUI/references.json). Distinguish:

- [WarcraftXL-Labs/client](https://github.com/WarcraftXL-Labs/client/tree/fdb9ad1819e68085d4cde4e33b2a17f772fcf85d):
  actual x64 source-client bindings in `WarcraftXL/Binding.cpp`, source seams,
  extended skin/index/bone paths, and a Gx adapter. Its source runtime calls do
  not require detouring a Windows executable. That does not make Windows DLL,
  MSVC, raw engine-layout or renderer dependencies portable.
- [Core v1.1](https://github.com/WarcraftXL/core/tree/f39aa1c9f2020d0968def3de895283cf0412e7cd):
  shared format contracts and BLP processing. Comments referencing a modern
  native reader are not its implementation: the snapshot contains the M2
  contract/particle/ribbon helpers but not the referenced `features/m2native`
  reader. Public default/main/v1.2 trees must not be described as containing it.
- [Core experimental](https://github.com/WarcraftXL/core/tree/2ae5bbb1c8b4c4a160352bcd7d990dc90d1fa93f):
  an older, separately pinned implementation with `structure/M2/M2Translate.cpp`,
  `structure/ADT/AdtMerge.cpp`, `structure/WMO/WmoTranslate.cpp` and corresponding
  runtime hooks. This is the actual translator source reviewed here. It uses
  WraithEngine namespaces/copyright and GPL-3.0-or-later headers; no code is
  imported. Its 32-bit Windows detours, patching, SEH and pointer-bearing packed
  structures cannot be brought into the native client unchanged.

The useful architecture is explicit source-level calls before pointer relocation,
skin finalization and render/material preparation, with a separate read-only
bundle resolver. Reuse bounded format knowledge through our own native interfaces;
review licences before any later import. Do not import MinHook, fixed addresses,
NOP version checks, exception-swallowing guards or a silent material fallback.

## Compatibility inventory

| Area | Source-backed finding | Native requirement / unsupported boundary |
| --- | --- | --- |
| M2 container/geometry | Experimental [Source.hpp](https://github.com/WarcraftXL/core/blob/2ae5bbb1c8b4c4a160352bcd7d990dc90d1fa93f/src/structure/M2/Source.hpp) and [TranslateM2](https://github.com/WarcraftXL/core/blob/2ae5bbb1c8b4c4a160352bcd7d990dc90d1fa93f/src/structure/M2/M2Translate.cpp) accept MD21/inner MD20 versions 272–274; target contracts name 264. Cameras compact 0x74 to 0x64 with a substituted fixed FoV. Particle records and animation IDs are also altered. | Introduce a distinct versioned reader, not a raised legacy version limit. Validate every chunk/array/stride and retain original bytes. A fixed FoV or removed particle feature is a declared loss, not retail parity. |
| SKIN data | [Loader.cpp](https://github.com/WarcraftXL/core/blob/2ae5bbb1c8b4c4a160352bcd7d990dc90d1fa93f/src/runtime/m2/Loader.cpp) rebuilds texture-unit lookups, submeshes, batch/material contracts at skin finalization. Later [M2Format.hpp](https://github.com/WarcraftXL/core/blob/f39aa1c9f2020d0968def3de895283cf0412e7cd/src/engine/assets/shared/models/m2/M2Format.hpp) describes `(level << 16) | indexStart`; the source client extends index pools/bone palette paths. | Bounds-check full index spans, bone combinations and palette budgets before allocation/draw. Preserve wide arithmetic and signed-relative ownership. SFID-based skin lookup is not supplied by the inspected experimental translator; require an explicit bundle mapping. |
| External SKEL | Later contracts expose bone records and appearance skeleton FileDataIDs, but the inspected translator has no SKID/SKS1/SKB1/SKA1/SKPD reader. | A split-skeleton model cannot be qualified from these sources alone. Add a separately bounded reader for the selected asset's actual chunks, parent skeleton chain, sequence tables and track ownership. Detect missing/cyclic parent dependencies. |
| External ANIM | [Anim.cpp](https://github.com/WarcraftXL/core/blob/2ae5bbb1c8b4c4a160352bcd7d990dc90d1fa93f/src/runtime/m2/Anim.cpp) strips an AFM2 header at the Windows read-completion detour. It relies on raw record offsets and SEH; it moves `fileSize-8` bytes without establishing a general modern animation format. | Implement a real bounded container parser before legacy rebasing, checking declared chunk lengths and every timestamp/value span. No AFID or arbitrary skeleton/animation bundle resolution is established by the detour. |
| Hierarchy/tracks | [Client.hpp](https://github.com/WarcraftXL/core/blob/2ae5bbb1c8b4c4a160352bcd7d990dc90d1fa93f/src/structure/M2/Client.hpp) / later M2Format specify parent indices, pivots and per-sequence timestamp/value arrays. TranslateM2 remaps selected newer animation IDs into a client range capped at 505. | Validate acyclic parent indices, sequence aliases, timestamps, interpolation/key shapes, global sequences and coordinate conventions. Keep each track's base owner explicit; do not repoint data outside the M2 signed-relative arena or silently remap unrelated gameplay animations. |
| Texture references/FileDataIDs | TranslateM2 scans TXID and appends resolved type-0 texture paths through a ResolveCtx. Core's [StorageApi.h](https://github.com/WarcraftXL/core/blob/f39aa1c9f2020d0968def3de895283cf0412e7cd/include/wxl/StorageApi.h) describes family-level storage selection. | A path table is not CASC/build/locale resolution. Use a pinned, hash-listed local bundle with explicit ID-to-file mapping and no signed-original mount override. Reject unresolved required IDs. |
| BLP variants | [BlpTranscode.cpp](https://github.com/WarcraftXL/core/blob/f39aa1c9f2020d0968def3de895283cf0412e7cd/src/engine/assets/shared/textures/blp/BlpTranscode.cpp) reads BLP2 headers and DXT mip data; converts encoding-3 BGR(A) to DXT5 and supports paletted output. It explicitly distinguishes BLP1. | Native decoding may preserve the source encoding rather than lossy offline conversion. Validate mip offsets, dimensions and compression independently. This source does not establish general BC5/BC7/HDR support. Respect actual Gx upload/ownership capabilities. |
| Character appearance/equipment | [AppearanceApi.h](https://github.com/WarcraftXL/core/blob/f39aa1c9f2020d0968def3de895283cf0412e7cd/include/wxl/AppearanceApi.h) describes display/geoset/texture and attached component/skeleton identities; the [source client binding](https://github.com/WarcraftXL-Labs/client/blob/fdb9ad1819e68085d4cde4e33b2a17f772fcf85d/WarcraftXL/Binding.cpp) exposes existing native model operations. | Modern ChrCustomization/DB2 choices, component meshes, composite skin textures and attachment/bone mappings require separately sourced versioned data. A working model viewer does not make newer appearance fields valid in 12340 packets or character creation. |
| Materials/renderer | Experimental Loader decodes modern shader IDs and rebuilds materials, clamps unsupported blends, splits passes and expands ribbon material arrays. The later source client adds blend mode 7 and wider indices/palettes. | Audit the selected model's actual batch/shader requirements against direct Vulkan / #134. Reject unsupported required effects truthfully; do not discard them, claim material parity from geometry, raise qualified limits, or extend OpenGL. |
| ADT | [AdtMerge.cpp](https://github.com/WarcraftXL/core/blob/2ae5bbb1c8b4c4a160352bcd7d990dc90d1fa93f/src/structure/ADT/AdtMerge.cpp) merges root/`_tex0`/`_obj0`, resolves modern texture and placement IDs, rebuilds legacy tables and emits a private ATSC texture-scale extension. | Useful read-only runtime assembly reference. Terrain scales, flags, placement identities, liquids and doodad/WMO dependencies require explicit native contracts. Private ATSC consumption is not original ADT compatibility. Separate from the one-character slice. |
| WMO | [WmoTranslate.cpp](https://github.com/WarcraftXL/core/blob/2ae5bbb1c8b4c4a160352bcd7d990dc90d1fa93f/src/structure/WMO/WmoTranslate.cpp) rebuilds MOTX, normalizes MOBA, converts MPY2, filters/rebuilds group chunks and emits WMP1 extra-pass plans. [Source.hpp](https://github.com/WarcraftXL/core/blob/2ae5bbb1c8b4c4a160352bcd7d990dc90d1fa93f/src/structure/WMO/Source.hpp) names discarded GFID/MODI/MOSI/MOUV data and fallback textures. | Resolve group/doodad/skybox IDs natively; preserve required data or report unsupported. Stripping chunks, promoting a texture or substituting a fallback is not full WMO/material compatibility. Separate workstream. |

## Smallest next displayed/animated character task

Implement **one read-only, versioned modern character bundle reader and existing
Model2 viewer adapter**, with a narrow explicitly declared format/material subset.
First inventory one locally supplied real character bundle, pin product/build,
locale, all resource IDs and hashes, and choose an asset whose required effects
the maintained renderer can represent. No newer installation or extracted modern
character bundle is presently an input to this work; source review alone cannot
select a qualified specimen or establish its external-skeleton requirements.

The task must include enough native infrastructure to display **and animate**
that specimen: bounded MD21/MD20 and one SKIN reader, explicit texture mapping,
and its idle sequence's skeleton/track resources. Prefer an embedded-skeleton
specimen if the real inventory supports one. Otherwise include exactly the SKEL
parent/chunk and ANIM variants the selected model needs; do not claim a geometry
reader alone completes this task. Map validated geometry, bone hierarchy,
attachments and tracks into a separately owned runtime representation before
connecting existing Model2 scene/draw interfaces. Preserve legacy near-allocation
and signed-relative invariants; do not rebase arbitrary external allocations.

Acceptance: unchanged source assets and clean 12340 baseline, explicit errors for
unsupported required chunks/materials/IDs, authored malformed/overflow/lifetime
tests in Actions, all four Linux profiles and Windows regression, real Linux
Vulkan pixels plus a measured idle animation, retained reference capture and
asset identities without redistributing assets. Compare bind pose, chosen
sequence, skinning, material/alpha and texture output. Actual ARM64 execution is
a later separately evidenced requirement, not inferred from compilation. A
12340 equipment/customisation mapping follows the viewer; no protocol extension
or gameplay substitution is included in this task.

Keep optional inputs in an external manifest (schema, product/build/locale,
source IDs/paths, SHA256, declared format variants and required renderer
capabilities). Never overwrite signed archives, interface keys or installed
files. Cache only derived runtime data under external state with provenance;
retain native source bytes rather than requiring destructive conversion.
