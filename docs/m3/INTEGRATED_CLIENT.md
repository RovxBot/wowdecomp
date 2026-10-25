# Integrated Linux startup

The opt-in `WOWDECOMP_BUILD_NATIVE_CLIENT` target `WowNativeClient` and
`linux-client` / `linux-client-asan` presets connect qualified native archives,
retained reads, SDL/Gx/Gxu, original FrameScript/XML/layout/Frame and images.
All compilation runs in Actions; Windows remains a separate regression target.

**Login rendering is incomplete.** The [Model2 loading continuation](#model2-loading-continuation--2026-10-25)
at `8a00e6203b8fc54bf74a886f9ee95a8bd1067931` qualifies retained original
file/async entry points and signed-relative model storage. Its downloaded
executable still stops at the actual ModelFFX factory. See the
[separate continuation record](integrated-model-io-validation.json).

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

The next concrete source group is original `CSimpleModel`/`CSimpleModelFFX`,
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
them; registering only a frame tag would leave those dependencies unresolved.

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
