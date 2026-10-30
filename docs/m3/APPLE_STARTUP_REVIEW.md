# Pinned Apple startup/platform review — 2026-10-25

For [particle startup](https://github.com/RovxBot/wowdecomp/issues/124), the pinned
jerrywossion `ShaderEffectManager.cpp` retains SFile close, bounds effect files at
1 MiB, checks complete reads and handles absent shader passes. Its World caller
loads Model2/Particle before scenes. Our native adaptation uses the existing
authenticated SFile owner, validates the complete bounded WFX grammar before
publication, and rolls back installed effects on failure. The original two-stage
effect storage stays separate from the one-sampler device limit. No Apple source
body, dependency or notice is imported; no Apple execution is established.

For [scroll-child rendering](https://github.com/RovxBot/wowdecomp/issues/122),
the pinned jerrywossion `Engine/Source/Frame/CSimpleScrollFrame.cpp` retains the
original clipped projection/viewport and no-argument child draw. It provides no
native retained-batch lifetime admission for our guarded Linux renderer. The
same file at the pinned WoWReMac repository returned 404 during this review;
earlier inspected references remain historical. This continuation adapts our
original traversal through existing root/batch owners, with precollection and
exception restoration. No Apple source, dependency or notice changes are imported.

For [FFX filter continuation](https://github.com/RovxBot/wowdecomp/issues/118),
the same jerrywossion pin's `PostProcessShader.cpp` recognises original Box4,
Gauss4 and Glow bytecode by metadata/digest before choosing Metal equations.
The same WoWReMac pin's `MetalFixedFunction.cpp` supplies independent UV varyings
to its translated fragment pipeline; `CGxDeviceMetalNative.mm` binds its shader
resources. These require Metal and do not provide a one-sampler Linux fallback.
Our adaptation retains the original ARB filter instructions and aliases only
the identical input bindings, with four separately checked UV streams. No source,
dependency or notice change is imported from either reference. Distinct-input
glow is not qualified by this aliasing. The default Glow continuation reuses this
fork's retained floating-point composition owner. It follows the reference's
bounded fingerprint dispatch approach, independently fingerprinting inspected
12340 ARB programs rather than using the reference's ps_2_0 digests. The original
RGB equation and alpha one are retained across single-sampler passes. No source
body or dependency is imported. The pinned WoWReMac repository could not be
re-read during this continuation (GitHub returned 404); its earlier review above
remains historical evidence, with no new Apple execution claim.

Both pinned 12340 references were inspected before equivalent implementation.
This batch imports **no source or dependency changes from either port**. It adapts
this fork's original sources and qualified services, with no new donor address/
verification claim.

The [FFX framebuffer copy](https://github.com/RovxBot/wowdecomp/issues/116)
additionally reviews `TexCopy` in the same pinned jerrywossion
`Engine/Source/Gx/Metal/CGxDeviceMetal.mm` and WoWReMac
`Engine/Source/Gx/CGxDeviceMetal/CGxDeviceMetalNative.mm`. Both use retained Metal
back/default color storage, explicit destination levels/planes and their Metal
copy/stretch helpers. These require Metal resources and cannot serve Linux.
The Linux implementation instead adapts this checkout's original
`CGxDeviceOpenGl::TexCopy` (12340 `0x0069D750`) through the existing
`GlRenderTargets` owner, retaining bottom-left coordinates and bounded immutable
storage. No Apple source bodies, dependencies or new provenance claims are
imported. Runtime and subsequent FFX shader acceptance remain in #116/#96.

| Pin and inspected paths | Direction and disposition |
| --- | --- |
| [jerrywossion `7489b5f2df6628a434b2ad9aceed66c0f2e6a431`](https://github.com/jerrywossion/wowdecomp/tree/7489b5f2df6628a434b2ad9aceed66c0f2e6a431): `CMakeLists.txt`, `cmake/Apple.cmake`, `Tools/AppleBootstrap/ClientHost.mm` | Coherent source groups connect original engine callbacks/Event/Glue to a main-thread host. Follow this direction. Replacing our corrected Foundation/archive/Gx owners would discard existing qualification. No wholesale merge/import. |
| Same pin: `Engine/Source/Os/Apple/OsCall.cpp` | Per-thread diagnostic contexts detach before deletion. The SDL loop currently does not need the original OsCall scheduler contexts. Not copied/selected. |
| Same pin: `WoW/Source/WowServices/Apple/ClientSettings.mm` | External roots, component validation and atomic replacement preserve read-only data. Use our qualified resolver for original CVars; no independent Cocoa provider/import. |
| [WoWReMac `3c7f82c02fa9d4c8399b8f95d30ef0df575ef069`](https://github.com/fmustafayaman/WoWReMac/tree/3c7f82c02fa9d4c8399b8f95d30ef0df575ef069): `WoW/Source/Mac/Main.mm` | Storm precedes `CommonMain`, with an 8 MiB main stack/native initialization. Retain these dependencies and explicit x86 floating point. AppKit/QoS/bundle paths are Apple-specific. No import. |
| Same pin: `Platform/Mac/Prefix.h`, `Platform/Mac/CMakeLists.txt`, `Platform/Mac/Paths.cpp` | Bounded CRT helpers can avoid replacement modules. Blanket prefix/Win32 shims need width/ABI review. Foundation/bundle path APIs cannot serve Linux; explicit native roots already cover it. No import. |

Their Metal paths need a separate Linux renderer. Preserve our SDL/OpenGL and
original `0x10003` UI shaders. Original Model/ModelFFX needs actual model/material/
shader and signed-relative-addressing integration; scalar inspection is not
animated rendering. Cocoa/Metal are unavailable on Linux, and neither reference
is a qualified drop-in replacement for our service ABI.

Follow-up model-boundary inspection read the first 100 lines of both pins'
`Engine/Source/Frame/CSimpleModel.cpp` and `WoW/Source/Glue/CSimpleModelFFX.cpp`,
plus jerrywossion's `M2Shared.cpp`, `M2Scene.cpp` (first 100 lines) and full
`Engine/Source/Model2/M2Memory.h`. The Frame entry still calls the actual Model2
scene; ModelFFX retains FFX/glow and sound-event dependencies. Jerrywossion's
model-root VM allocation keeps external arrays within signed 32-bit offset range
on Apple/sanitizer heaps; its non-Apple branch retains aligned heap allocation.
This is useful ownership/addressing guidance, not a Linux allocator import or a
qualified shortcut around rendering. No model source was imported in this batch.

The loading continuation additionally inspected the complete pinned jerrywossion
`Engine/Source/Model2/M2Memory.h` and `Engine/Source/Os/Apple/OsMemory.cpp`.
The Apple allocator searches VM holes without replacing occupied mappings and
keeps model roots outside distant sanitizer heaps. This continuation imports no
implementation bodies: it uses our existing `ModelArena` reservation for roots
and external arrays together, retains each reservation through outstanding
payloads, and adds native checks to original `M2Array`. Its Linux implementation
does not depend on Mach, Cocoa or Metal. The original async cancellation/ownership
callers in this checkout informed the retained `CAsyncObject` adapter; none of
the pinned ports' archive/async or authentication code is selected.

Bundled Lua/Expat/FreeType/MPQ codecs and pinned SDL 3.2.28/notices remain selected.
Inspected reference trees contain third-party notices; no root license was found
establishing an independent blanket relicensing assumption. Before later imports,
record exact paths/revision, original provenance, notices, dependencies and checks.

Reported Apple world execution does not establish unchanged GrimGuzzler challenge/
Warden compatibility. Separate Warden/Epoch branches do not qualify these pins.
No challenge omission, integrity fabrication or server-policy change is imported.

## FFX render-target continuation — 2026-10-27

Before connecting original FFX depth-target flags, the same pinned references
were inspected for offscreen color/depth ownership:

| Pin and inspected paths | Direction and disposition |
| --- | --- |
| jerrywossion `7489b5f2df6628a434b2ad9aceed66c0f2e6a431`: `Engine/Source/Gx/Metal/CGxDeviceMetal.mm`, `ApplyRenderTarget` / `DeviceSetRenderTarget` | An offscreen color target without explicit depth retains size-matched Metal scratch depth (`Depth32Float`). This supports keeping attachment lifetime with the renderer; no source imported. |
| WoWReMac `3c7f82c02fa9d4c8399b8f95d30ef0df575ef069`: `Engine/Source/Gx/CGxDeviceMetal/CGxDeviceMetalNative.mm`, `ITargetsFromState` / `ITextureDepth` / `DeviceSetRenderTarget` | Target extent selects retained offscreen depth or the window depth. Explicit depth has its own ownership. No source imported. |
| Same WoWReMac pin: `Engine/Source/Gx/CGxDeviceMetal/MetalResources.mm`, `Texture::Create` / `Texture::CreateTarget` | Private Metal textures retain extent, levels, render-target usage and optional resolve storage. These Objective-C++/Metal dependencies cannot serve Linux. No source or dependency imported. |

Our original OpenGL `IDeviceSetRenderTargetFBO` identifies bit 8 as an implicit
depth-24 renderbuffer request. The already-qualified Linux `GlRenderTargets`
owner retains that exact depth format for renderable storage. The continuation
therefore admits the original flag through `LegacyDevice` with its existing
owner and feedback checks; it does not copy an Apple backend or alter device
limits. There are no new provenance, notice or dependency selections in this
batch. This review does not establish original ModelFFX painting or macOS
graphics execution.

The model-paint bounds follow-up also inspected both pins'
`Engine/Source/Model2/CM2SceneRender.cpp`, specifically `DrawBatch` and
`DrawBatchDoodad`. Both retain direct palette writes followed by an Unlock; they
do not supply a range preflight for this fork's 94-register adapter. Their GPU
paths therefore cannot be used as evidence that our CPU-loaded model fits those
registers. No source was imported. The native continuation validates the complete
write span through the existing device before exposing shadow storage; selecting
the original CPU rendering/material path remains separate work.

## CPU model/material continuation — 2026-10-27

Re-read `SetBatchVertices` in both pinned `CM2SceneRender.cpp` files above.
Both select CPU vertex transforms with `CShaderEffect::s_enableShaders == 0`;
cache-only CPU selection does not change that dispatch. Also inspected the pinned
WoWReMac `CGxDeviceMetalNative.mm` fixed-vertex selection (around lines 2725–2785):
it builds material/light/texture keys when the vertex shader is absent. That
Metal pipeline requires Objective-C++ and its Metal compiler/resources; it cannot
serve this Linux OpenGL target. No source or dependency is imported.

The continuation adapts this checkout's original OpenGL `IStateSyncLights`,
`IStateSyncMaterial`, texture environment operations and CPU `CM2Model` path
through the existing native device/buffer/transform owners. Qualified limits stay
at 94 constants and one texture unit; original palette preflights stay enabled.
Compilation, authored CPU pixels and original-data rendering require separate
validation. Bundled dependency selections and notices are unchanged.
