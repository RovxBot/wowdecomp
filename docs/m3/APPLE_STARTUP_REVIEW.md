# Pinned Apple startup/platform review — 2026-10-25

Both pinned 12340 references were inspected before equivalent implementation.
This batch imports **no source or dependency changes from either port**. It adapts
this fork's original sources and qualified services, with no new donor address/
verification claim.

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
