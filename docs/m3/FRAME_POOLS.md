# Native pooled Frame allocation

This selects the original `Create_SimpleFrame` allocation entry and adds a typed
FontString constructor against the original class `CDataAllocator` pools. The
base factory moves unchanged for Windows into `FrameXML/CreateFrame.cpp`.
The native factory registry, region/frame Lua methods, XML loading, template and
language processing remain subsequent callers. No donor or dependency changes.

Selected factories validate owner/current-context, parent and traversal state
before allocation. Construction uses the original class size and block counts
(100 Frames, 809 FontStrings); a throwing constructor returns its slot after
completed base destructors run. Native storage admission checks the original
pointer-header and byte-stride alignment. Class delete identifies exact slots in
its own blocks and returns them to that pool; ordinary heap objects and subclasses
retain matching global deletion. Parent/root ownership and deferred destruction
are shared across both allocation paths. External pointers remain borrowed.

Pool registrations belong to the existing `Fonts` owner. All objects and raw
allocated slots must close before that owner; it clears blocks only after its
close preflight and named-font/metatable cleanup. Registration does not retain
objects or authorize raw public allocator use outside the selected constructors.
Do not clear class allocators independently or free pools with live objects.
Finish consumers/root before renderer/font factories, Gxu/layout/XML/VM and the
device/context/file queue. Public allocator fields retain the original layout.

`NativeFrameHierarchyCheck` runs pool checks in each of three owner cycles,
including growth beyond both original block sizes, native alignment, slot reuse,
throwing derived construction, mixed heap/pool recursive and deferred deletion,
actual pooled FontString rendering and exact repeats, invalid/foreign/callback
factory rejection, raw-slot close rejection, root teardown and owner block release.
Existing hierarchy/input/window checks remain selected. Hosted release and strict
ASan/UBSan/leak builds and full signature-enabled Windows compilation are required;
all client and dependency compilation runs in GitHub Actions.

Original-data execution uses `/mnt/games/3.3.5aClean/Data` read-only with separate
settings/cache roots and original-key/selective MD5 policy. GlueFonts subset
signature checks remain separate from complete signed ordered GlueXML creation
and the full post-creation digest gate. Pooled Lua/XML registration, remaining
widgets, physical mouse/IME/edit-box acceptance, original login rendering,
transport/authentication and observed gameplay remain open. M3 is incomplete.
Preserve historical evidence, server revision discrepancies and interactive sign-in.

## Qualification

[PR #64](https://github.com/RovxBot/wowdecomp/pull/64) is qualified at
`d2088a7cab528ca915b5fb1faa3cb354048ebd0a`, merged as
`f3e74f19562583ded6c8aeb414f4dccf60782da0`. The [durable record](frame-pool-validation.json)
retains all thirteen passing implementation PR workflows, 38 CTest checks per
Linux release/strict ASan/UBSan/leak configuration and full signature-enabled
Windows x64 compilation at matching-tree checkout
`eb7985d3665dcbd81f9c1c40b9be4543938f15ab`. Three archive digests, all 51 packaged
executable hashes/types and notices verified. Graphics remain Linux x86-64;
prerequisite matrices retain separate actual ARM64 macOS and Windows coverage.

Downloaded RX 7900 XTX / Mesa 26.2.5 / SDL X11 packages passed 30 authored/original-
data input/UI/resource executions, three owner cycles each, with empty stderr and
zero workers. Each of four target/transform/buffer/texture fixtures passed three
cycles per mode. All fifteen release/sanitizer capture pairs match; final original-
data hierarchy/input captures retain SHA-256
`55b0e2c731d888293851e66a21b2b7eccaa29405097fb4b6b1e65588f60c93cf`.
The inspected final capture is authored white hierarchy text with original fonts/
UI shaders. Intermediate readbacks assert pooled FontString rendering and exact
repeats; those buffers are not exported captures or original login UI.

Each hierarchy/input cycle passed growth to 101 Frame and 810 FontString slots,
alignment/reuse, throwing construction, mixed heap/pool recursive/deferred/root
teardown, factory guards and owner block release. Original-key signaturefile and
selective MD5 checks remained enabled; original-data cycles authenticated GlueXML
before/after eighteen GlueFonts definitions. Complete ordered creation and its
full post-creation digest gate remain open.

Local source/launcher checks: 57 passed, one unavailable-CMake skip. Original
Windows-selected tokens and the extracted base factory match immutable pre-PR
source; whitespace passed. All compilation ran in Actions. The initial restricted
desktop launch failed because X11 was unavailable; the same hash-verified release
binary passed with desktop access. Its report remains failed evidence. No assertion
or detector was disabled; no hosted implementation failure occurred. Duplicate push
runs remain separate from the thirteen PR qualification runs.

## Next original callers

1. Select original region/frame/title/FontString script metatables and route the
   Lua `CreateFontString` allocation through the qualified typed constructor.
   Preserve canonical live Lua tables and reject unavailable consumers before
   stack/name/taint mutation. Native nonempty input handlers remain unavailable.
2. Connect the original FrameXML factory registry, named-parent lookup, retained
   virtual templates/inheritance and language processing. Keep XML nodes alive
   through consumers and close external script references before factory teardown.
3. Adapt each remaining widget class's allocator/deletion before selecting its
   factory. Texture/image-mode, backdrop, scroll, animation/model and edit-box
   consumers remain open; the base Frame pool does not qualify those classes.
4. Execute complete signed ordered GlueXML and its full post-creation digest gate,
   then render actual login before interactive user sign-in. Physical mouse/IME/
   language/edit-box/candidate UI acceptance and transport/gameplay remain open.

No new physical-input, Windows runtime, macOS graphics or server acceptance is
recorded. M3 stays incomplete; preserve signatures, server revision discrepancies
and interactive user sign-in.
