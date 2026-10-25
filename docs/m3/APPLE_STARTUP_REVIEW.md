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

Bundled Lua/Expat/FreeType/MPQ codecs and pinned SDL 3.2.28/notices remain selected.
Inspected reference trees contain third-party notices; no root license was found
establishing an independent blanket relicensing assumption. Before later imports,
record exact paths/revision, original provenance, notices, dependencies and checks.

Reported Apple world execution does not establish unchanged GrimGuzzler challenge/
Warden compatibility. Separate Warden/Epoch branches do not qualify these pins.
No challenge omission, integrity fabrication or server-policy change is imported.
