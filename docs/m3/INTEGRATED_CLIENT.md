# Integrated Linux startup

The opt-in `WOWDECOMP_BUILD_NATIVE_CLIENT` target `WowNativeClient` and
`linux-client` / `linux-client-asan` presets connect qualified native archives,
retained reads, SDL/Gx/Gxu, original FrameScript/XML/layout/Frame and images.
All compilation runs in Actions; Windows remains a separate regression target.

**Login rendering is incomplete.** The [original model factory continuation](#original-model-factory-continuation--2026-10-26)
selects actual Model/ModelFFX, Model2 scene/cache/shared loading, Camera,
ShaderEffect and FFX sources. Downloaded execution now passes the ModelFFX
factory and exposes subsequent original AccountLogin dependencies. Its
[separate evidence record](integrated-model-render-validation.json) distinguishes
authored factory/loading checks from actual original-data startup and rendering.

The earlier downloaded release and three strict
ASan/UBSan/leak executions at implementation
`5a4c2d2f03a58403e5085c620e6b8cd8389a861a` (Actions checkout
`86abd713d7469162f4cd8f93c926fdadf28d65db`, matching tree
`41b9c739a3e7fb2968f0327f1f3e767d2e9311a2`) authenticate the original archives
and 60 GlueXML visits, execute through RealmList/AddonList and stop before
constructing `ModelFFX AccountLogin` in `AccountLogin.xml`. Each exits 1 with
zero workers after cleanup; the strict runs have no sanitizer finding. The full
creation digest gate stays enabled but has not been reached. No original login
frame, physical input acceptance, authentication or gameplay is established.
[Durable validation](integrated-client-validation.json) retains exact archives,
executable hashes, hosted identities, failed attempts and separate fixture results.

All 14 implementation workflows (47 jobs) passed at that source, including full
signature-enabled Windows x64 compilation and actual ARM64 prerequisite execution.
Both Linux GL configurations passed 38 CTest checks. Downloaded matching release
and strict Frame/image fixtures also passed three original-data cycles each on
RX 7900 XTX / Mesa 26.2.5 / SDL X11, with empty stderr and zero workers. These
fixture results include the changed factory/callback and image-pressure contracts;
their authored composition/input and AccountLogin field subset stay separate from
the integrated executable's ModelFFX failure. No new physical-input acceptance or
complete login capture is recorded.

This is [PR #84](https://github.com/RovxBot/wowdecomp/pull/84), integration work in
progress. It does not yet execute `CommonMain`, complete `InitializeEngineCallback`
or full `CGlueMgr::Resume`, authenticate or enter the world. The entry adapts their
startup dependencies and original Glue interface-creation gate. Actual execution
must establish complete login rendering; fixture/window success does not.

Storm, floating point, an 8 MiB main stack and character conversion precede SDL
and workers. The C numeric locale remains intact. Explicit settings/cache roots
stay outside both read-only installations. Original CVars use original `SET`
syntax in external `WTF/Config.wtf`. Native files admit `OS_FILE_TYPE_ANY`;
other extended types remain unavailable. Native CVar reads bound sizes/short reads.

The Event adapter retains original IDs, priority/equal-priority ordering,
marker-based listener mutation and stop propagation on the SDL owner thread.
Active dispatch cannot destroy its context. No network scheduler is installed.
Input retains the existing SDL focus/IME/capture/lifetime contracts.

The ordered TOC/Include/Script/Font/template/frame path feeds actual original read
bytes into the creation digest. Read/script/factory errors, cycles and excessive
lines fail visibly and unwind trees/recursion state. Original InterfaceKey checks
precede creation; the full post-creation digest comparison remains mandatory.
Protected Lua errors are observed without replacing their handler. There are no
skipped widgets, modified original Lua/XML, alternate keys or unchecked modes.
SimpleHTML/hyperlink/ScrollFrame/Slider form the first startup-driven source batch.
Aligned pools retain matching deletion; HTML text closes before private fonts.

Close frames/callbacks before widget metatables, Scripts/templates, images,
renderer and Fonts/Gxu/layout/device/context. CVar strings close before XML's
string manager, then XML/VM, retained queue/packages and Storm/time. Existing
ownership guards remain authoritative. Authentication, server/Warden policy and
interactive sign-in remain unchanged.

Original OnLoad may set existing visibility/layout, Button state/font selection,
IDs and scrollbar values in the scoped Glue load phase. The checked `CreateFrame`
transaction may construct and unwind newly allocated objects there, including a
failure after the constructor. A nested callback adds a pin and loses the factory
capability until it returns; reparenting/deleting existing callback objects stays
blocked. Root traversal, actual resize and input guards remain enabled. This is
not general allocation from interactive callbacks. Images now admit 1,024 retained
entries independently of their unchanged 256 MiB pixel budget; Event polling
delivers completions before update/paint. Queue shutdown precedes package-slot
destruction even when archive startup throws or times out.

AddonList's pre-authentication `GetNumCharacters` reads the original
`CCharacterSelection::s_characterList`; a focused fixture changes the list and
checks the count and reset. The fixed-width `CHARACTER_INFO` declaration is
separated from socket/client headers without changing its fields or signatures.
This selects list state and its count query only. Native model/component producers,
display release, character enumeration and other character-selection callbacks
remain unavailable; a resource-bearing row cannot silently pass native teardown.

## Downloaded execution

Verify an Actions archive digest and extract outside both reference installations:

```sh
python3 Tools/Porting/run_native_client.py \
  --artifact /tmp/extracted-native-linux-client \
  --data /mnt/games/3.3.5aClean/Data \
  --settings /tmp/waystone-settings --cache /tmp/waystone-cache \
  --frames 3 --output /tmp/waystone-startup.json
```

Add `--sanitizers` for the matching strict package. Omit `--frames` for a session
closed through window controls once startup completes. The launcher checks ELF/
binary/notice hashes and records identities, stages, errors and timeouts. It pins
X11/EGL-X11 and preserves ASan/UBSan/leak checks. No credentials are supplied.

Initial execution authenticated 28,948 archive records and 60 GlueXML visits,
created the original root and stopped at unavailable SimpleHTML in `GlueDialog`.
No original login frame was presented. Three initial strict desktop runs also
reported a 33-byte Xlib title-conversion leak before character-locale initialization;
these remain failed evidence. Subsequent compile/CVar-contract failures and exact
source/build/runtime identities are recorded separately as integration proceeds.

The loading continuation initially identified original `CSimpleModel`/`CSimpleModelFFX`,
Model2 scene/cache/model/shared loading, Camera, ShaderEffect and FFX/glow. Original
`AccountLogin_OnLoad` selects camera/sequence and loads
`UI_MainMenu_Northrend.m2`. This requires the real model's retained read/animation
callbacks and signed relative allocations, plus device streams/materials/shaders
and FFX targets. The selected native legacy device is qualified for UI batches,
advertises one texture unit and rejects fixed-function lighting/fog. Model shader
and material coverage remains unqualified. Scalar model inspection does not supply
this path. The pinned-port
model review is recorded in [the Apple review](APPLE_STARTUP_REVIEW.md); its Metal
backend cannot directly provide Linux rendering. Full engine/Glue startup and
additional script/platform services remain to be connected as this path reaches
them. The following factory continuation selects these real callers together;
their device/material/rendering requirements remain open.

## Original model factory continuation — 2026-10-26

The existing integrated target now links the original Frame Model/ModelFFX
factories and script tables, Model2 cache/model/shared/scene/render consumers,
Camera, particles/ribbons, ShaderEffect and FFX/glow as a coherent source group.
This reuses reconstructed product sources and qualified native services. The
[pinned Apple review](APPLE_STARTUP_REVIEW.md) informed the boundaries; no Apple
source bodies, dependencies or new donor verification claims were imported.

Model roots remain in the retained native reservation. Skin/animation and expanded
combo arrays use checked near allocations without moving a root away from its
external arrays. Native relocation validates spans, alignment, nested track arrays
and inline/external sequence bases before access. Original native callbacks retire
their async object before initialization and propagate initialization failures.
Suffix construction is bounded by the original filename storage. Initialized flags,
aligned allocation/deallocation and native `new`/`delete` pairs retain ownership.
The single-owner integration does not enable Model2's animation worker bit.

Factories initialize original Prop/Model2/ShaderEffect/FFX and real glow passes.
Partial FFX construction rolls back earlier shader-owning passes. Frames and their
callbacks close before these owners; the global FFX effect detaches before its
deletion. Model/cache/FFX owners close before scripts, images/fonts/device/context
and files. The original `(0, 0)` stream query now returns the active shared stream
without retiring its allocation. Queue scopes stay on the legacy object owner;
native workers own staged bytes only. Optional model animation audio returns
failure with a diagnostic until real audio is integrated.

The device still advertises its qualified **94 shader registers and one texture
unit**. Selecting model/material/FFX code or successfully constructing its shaders
does not qualify model rendering, additional samplers or full scene effects.
Original fog/glow XML is admitted through the real handlers. Integrated hidden
screens/regions exceeded the old layout population: the bounded limit is now
8,192 objects. Reading the original HTML then exposed the old 64-frame anchor
bound; the supplied credits contain more than 1,000 chained text regions. Anchor
paths are now bounded at 2,048, independently of the unchanged 64-level Frame
parent graph. The image pixel budget remains 256 MiB.
Original `.tga` image requests try the corresponding BLP basename unless exact
suffix bit 3 is set; there is no TGA decoder, fallback pixel or substituted asset.

Original SimpleHTML's locale files are a distinct read kind. The existing resolver
and retained worker read explicit `Data/enUS/*.html` basenames, with an 8 MiB cap,
case ambiguity and escaping-symlink rejection. Lua/XML/TOC bytes still come only
from authenticated packages; the locale reader cannot supply interface scripts.
As in original `CSimpleHTML::LoadXML`'s conditional `SFile::ReadFile`, a failed
optional HTML read leaves the widget empty; clean data has no `termination.html`.
The reader still returns failure, and no replacement bytes are generated.
Both reference installations remain read-only. HTML content and game data are
not copied into Git. Original ordered GlueXML/post-creation verification remains
mandatory, and no credentials or server/Warden policy changes are involved.

The existing `NativeStartupCheck` exercises real pooled ModelFFX construction,
injected shader-construction rollback/retry, FogColor/glow XML, guarded original
Lua model methods, Camera duplication, stream query identity, original M2 track
relocation and invalid model initialization through the real instance/cache/shared
read path. It verifies root retirement at owner shutdown. Its locale-file cases
cover exact bytes, source-kind separation, missing/large/traversing/escaping and
case-colliding files. HTML parser trees and temporary paragraph text now close
when content creation throws, preserving the underlying error. Existing layout
and image/hierarchy fixtures cover the new
population bound, partial row rollback and BLP cache identity. These authored
checks do not establish a rendered original model or complete login UI.

The [durable record](integrated-model-render-validation.json) retains individual
source/checkouts, jobs, artifact hashes, failures and downloaded desktop reports.
Earlier reports below retain their own historical ModelFFX stop; they are not
relabeled as results of this continuation.

At implementation `e07f774147c865bf087ded33683ed28ddfa68cb3`, one release and
three strict ASan/UBSan/leak-enabled original-data desktop executions authenticate
28,948 archive records and 60 GlueXML visits, construct ModelFFX and reach
`AccountLogin_OnLoad`. Each exits 1 at `AccountLogin.lua:15`: original Glue
`GetBuildInfo` is not registered. Each reports zero workers after cleanup; no
sanitizer finding occurs. **The full post-creation digest stays enabled and
unreached.** No original model or complete login screen has been rendered.

The corresponding four authored startup-check executions pass with empty stderr.
Both hosted startup configurations pass. Linux GL release/strict configurations
at `8e4f993ee0850c13674fea24460f50e63d52c7cc` each pass 38 CTest checks;
four native layout configurations each pass 15 checks, including actual ARM64
macOS and the 2,048 anchor bound. Full signature-enabled Windows x64 compilation
passes at `dc0b0ca6937bba26092ba0dc070344c861303da0`. Later edits are confined
to excluded native inputs or native conditional bodies; the record retains that
earlier Windows identity and the unchanged-input proof. The final source audit
passes separately. This is five selected workflows/ten jobs across their stated
checkouts, not a claim that all fourteen workflows ran at the final head.

Six downloaded archives, all 67 packaged executable hashes/types and their notices
are verified. Current release/strict image and hierarchy fixtures each pass three
original-data desktop cycles with empty stderr and zero workers. The GPU profile
is RX 7900 XTX / Mesa 26.2.5 / SDL X11. An earlier strict hierarchy execution
aborted on X11 `BadWindow`, then reported leaks during abrupt exit. Its symptoms
and report digests remain recorded; three identical-binary repeats and the later
matching checks pass without disabling detectors. The exact X11 cause was not
captured. No new physical-input, Windows runtime, macOS graphics or server
acceptance is established. PR #84 remains draft.

Continue with the original Glue script service group reached by login OnLoad.
Its `GlueScriptEvents_Scripts.cpp::Script_GetBuildInfo` returns **five** values:
localized `VERSION`/`RELEASE_BUILD` labels, then the original version, build and
date from `GameVersion.h`. The separate in-game API returns four values and is
not this caller's contract. Preserve original service/state behavior and the
mandatory creation digest as further calls become reachable. Then observe actual
Model2/material/shader/FFX update and paint through the truthful device limits;
factory success alone does not supply that rendering path.

## Model2 loading continuation — 2026-10-25

The integrated target now installs a bounded original `SFileImpl`/`CAsyncObject`
adapter over the existing retained `FileQueue`. Production opens read only from
authenticated packages. Model completions run outside the queue's callback pump,
allowing an original completion to open and enqueue its skin/animation member.
Native tickets remain private. Workers own staged bytes and never access legacy
objects, caller destinations or model callbacks. Short reads select the unload
callback without copying a partial model into the destination.

Cancellation retires the object/file on the owner thread and always returns the
original immediate-success value 1: the caller may free its destination, and the
deferred cleanup callback is not invoked. A retired native completion cannot copy
bytes or call a dead owner, including after adapter destruction. Close model
owners, async objects and file handles before the adapter, then the retained queue.
Local package opens/read/seek/size and async submit/poll/wait/cancel are selected;
streaming, host-file fallback, general queue/thread/progress services and other
legacy SFile operations are not provided by this adapter.

`OsNative::AllocModelRoot` reserves model roots and later `OsAllocNear` payloads
in the existing bounded `ModelArena`. Both allocation kinds retain the same
reservation, with 16-byte alignment and checked complete target extents. The
default reservation is 256 MiB, with no replacement mapping or unrelated-heap
fallback. Suballocation is monotonic until the reservation closes; exhaustion
fails. Retiring the root prevents new allocations while outstanding payloads
retain the reservation. Wrong-kind and foreign-thread releases fail visibly.
Native original `M2Array` keeps its 8-byte wire representation and now rejects
index overflow, misalignment and signed-displacement truncation before changing
an offset. Existing Windows implementation bodies remain selected unchanged.

The existing `NativeStartupCheck` exercises model-to-skin callback chaining,
short/empty reads, seek/count semantics, cancellation after staged completion,
unload, bridge destruction with a pending read, original relative-array access,
negative offsets, reservation exhaustion and owner/kind checks. This is a
loading prerequisite; `CM2Shared`/scene/animation, the actual ModelFFX factory,
Camera/ShaderEffect/FFX and model device rendering are still not selected. The
integrated executable still stops at AccountLogin's ModelFFX factory. Full ordered
post-creation verification remains enabled and unreached.

[Qualification](integrated-model-io-validation.json) records all 14 final-source
workflows / 47 jobs passing, including full signature-enabled Windows x64
compilation, actual ARM64 prerequisite execution and 38 CTest checks per Linux GL
configuration. Actions checkout `9003068169cba3efa7d8fa1d5ce4012549f969a5`
and implementation `8a00e6203b8fc54bf74a886f9ee95a8bd1067931` share tree
`47b2496f85d3b0d80b0f940248bcba964d71a921`. Two archive digests, four packaged
ELF executable hashes/types and all nine notices per package were verified.
One release and three strict downloaded startup-check executions passed with
empty stderr. The corresponding four clean-data executions authenticated 28,948
archive records and 60 GlueXML visits, then exited 1 at the same ModelFFX factory
with zero workers and no sanitizer finding. Those are observed failure paths,
separate from the authored loading checks. RX 7900 XTX / Mesa 26.2.5 / SDL X11
was used. No detector was disabled, no client/dependency compiled locally, and
both installations remained read-only. Earlier successful startup checks and the
superseded Windows/GL cancellations remain recorded separately.
