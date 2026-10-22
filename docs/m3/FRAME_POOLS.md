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

Hosted and downloaded validation will be recorded after qualification.
