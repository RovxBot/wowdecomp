# Original Frame layout and anchors

This package follows the qualified [authored image mip/readiness work](IMAGE_MIPS.md).
It selects actual 12340 `CLayoutFrame.cpp` and `CFramePoint.cpp`, scalar coordinate
and rectangle/vector consumers under a separate native ABI. Original anchor names,
`FramePointToString` and `LoadXML_Dimensions` are moved unchanged from `LoadXML.cpp`
to shared `LayoutXML.cpp`, so Windows and native targets use the same bodies.
Native entry guards are adaptations; historical OFFSET/VERIFY tags describe the
Windows bodies, without a new binary verification claim. No donor code is imported.

`Frame::Native::Runtime` lives inside the XML and FrameScript owners. Its lease
prevents XML destruction while installed; the XML owner already pins FrameScript.
All layout mutation, queries and point-pool operations belong to that owner thread.
Finish resize callbacks, destroy frames and standalone points, then close layout,
XML and FrameScript, in that order. Destroying layout objects from resize callbacks
is rejected. Public legacy fields are borrowed owner-thread state, not a separate
thread-safe API. Root owners must supply their screen rectangle; this package does
not provide the actual screen root/factory or SDL size invalidation yet.

Native constructors initialize layout/point flags before reading them, use portable
RTTI names and clear the point pool after the last consumer. The Windows constructor
and allocator bodies remain selected for the original client. Unsigned native
anchor indices are checked before indexing, and nonfinite coordinates/scales,
negative dimensions and nonpositive scales are rejected. Zero dimensions retain
original anchor-derived size behavior; depth retains its original 0.2 minimum.
At most 1,024 live frames and 64 frames per anchor path are admitted. Proposed
edges are validated over the whole graph with memoization, rejecting cycles and
excessive depth before mutating anchors, including repeated references.

The original anchor solver, resize dependency registration, protection propagation,
deferred/pending ordering, scaling, clamping, flattening and XML relative lookup
remain in the actual consumers. Native origin destruction schedules dependents
through original resize lists. A failed previously valid rectangle now propagates
invalidation to its dependents, preventing stale transitive rectangles after the
pending queue is processed. Original absolute XML dimensions convert both axes
through aspect compensation and DDC width; relative dimensions retain raw values.
This is not a new pixel-only layout system.

[M3 layout Actions](../../.github/workflows/m3-layout.yml) compiles Linux release
and strict ASan/UBSan/leak, actual ARM64 macOS and Windows native configurations.
`NativeLayoutCheck` repeats three complete owner/pool cycles, exercising all nine
anchors, size inference, scale/depth/clamp, flatten/unflatten, protected anchors,
deferred/unloaded objects, dependency destruction, invalid inputs, cycle/depth
bounds, foreign-thread rejection, callback lifetime rejection and original XML
absolute/relative/setAllPoints behavior at 4:3 and 16:9. The exact CMake graph is
audited; original signatures stay enabled. The separate Windows baseline compiles
the full x64 client. All client/dependency compilation runs in Actions.

Qualification is pending hosted results. This authored layout fixture adds no
original-data execution, GPU/rendered output, physical-input, Windows runtime,
macOS desktop or server acceptance. Frame font/string/region/root factories,
disk/atlas/default image policies, actual GlueXML/login execution/rendering and
transport/gameplay remain open. M3 remains incomplete; preserve server revision
discrepancies and interactive user sign-in.
