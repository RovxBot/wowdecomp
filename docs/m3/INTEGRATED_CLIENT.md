# Integrated Linux startup

The opt-in `WOWDECOMP_BUILD_NATIVE_CLIENT` target `WowNativeClient` and
`linux-client` / `linux-client-asan` presets connect qualified native archives,
retained reads, SDL/Gx/Gxu, original FrameScript/XML/layout/Frame and images.
All compilation runs in Actions; Windows remains a separate regression target.

The native ScanDLL Lua bindings use the existing Glue script-service owner.
`ScanDLLStart` validates the original two-string API and queues one unavailable
`SCANDLL_ERROR` for the outer Event idle phase; repeated starts cannot duplicate
delivery. Shutdown cancels pending delivery before script/CVar teardown. No URL
is fetched, file written, Windows DLL loaded or scan worker created. Neither
`SCANDLL_DOWNLOADING` nor a successful `SCANDLL_FINISHED` result is fabricated.
`IsScanDLLFinished` remains nil. The original AccountLogin error handler calls
`ScanDLLContinueAnyway` automatically; native continuation therefore raises an
explicit unavailable error and grants no approval. Reentrant polling/closure
during event delivery is rejected. This is a failure-only capability boundary,
not an implemented native scanner or completed login lifecycle. The original
Windows download/signature/scan implementation remains separate; resolving the
native platform/integrity requirement is tracked in [#131](https://github.com/RovxBot/wowdecomp/issues/131).

Native model startup loads unchanged `Model2.wfx` and `Particle.wfx` through the
authenticated retained SFile adapter before constructing a scene. Files are
bounded at 1 MiB; parsing bounds names, scopes, arguments, effects and the original
single fixed/shader pass storage. Fixed passes retain up to two texture operations;
this does not advertise a second physical sampler. Missing files, short reads,
malformed/duplicate definitions and missing required effects fail startup and
retire partial model/effect ownership. CPU mode retains the original fixed
definitions. [Particle qualification](https://github.com/RovxBot/wowdecomp/issues/124)
tracks actual geometry, pixels, lifetimes and repeated clean-data frames;
[complete login rendering/input](https://github.com/RovxBot/wowdecomp/issues/96)
remains a separate requirement.

**Login rendering is incomplete.** The [original model factory continuation](#original-model-factory-continuation--2026-10-26)
selects actual Model/ModelFFX, Model2 scene/cache/shared loading, Camera,
ShaderEffect and FFX sources. Downloaded execution now passes the ModelFFX
factory. The [Glue service continuation](#original-glue-service-continuation--2026-10-26)
now completes original ordered creation and matches the mandatory post-creation
digest. The [activation continuation](#glue-activation-continuation--2026-10-26)
reaches the actual model-read barrier. The [native model continuation](#native-model-instance-continuation--2026-10-27)
selects original CPU skinning for that budget and passes model initialization.
The [login activation continuation](#original-login-activation--2026-10-27) now
completes original activation. The [FFX target continuation](#ffx-depth-target-continuation--2026-10-27)
passes its depth flag. The [model paint continuation](#model-paint-bounds--2026-10-27)
admits the cached rectangle read and rejects an oversized GPU palette before
writes. The [CPU material continuation](#cpu-modelmaterial-continuation--2026-10-27)
now connects original CPU vertices/fixed materials; original-data paint rejects a
second texture binding beyond the qualified one-unit device. Complete login/model
rendering remains open. Its historical
[separate evidence record](integrated-model-render-validation.json) distinguishes
authored factory/loading checks from actual original-data startup and rendering.

The initial integration record below is historical. Its release and three strict
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
The integrated loop checks protected errors after input and before clearing or
painting the next frame, as well as after the tick. A renderer failure must not
mask an earlier callback failure. Callback admission is tracked in
[#95](https://github.com/RovxBot/wowdecomp/issues/95); this diagnostic ordering does
not relax mutation or traversal guards.
The original scroll-range notification has a bounded existing-control capability:
qualified slider range/value, scroll offset, visibility and Button enable changes
may use the original pending-frame lists during update. Other state edits and
layout-script/construction/deletion guards retain their normal admission. Resize,
collection, rendering and foreign-thread access still reject these operations.
The notification completes synchronously in original callback order; capability
cleanup follows protected errors and native exceptions. Qualification and remaining
integration stops live in [#95](https://github.com/RovxBot/wowdecomp/issues/95).
Mouse-wheel dispatch separately pins layout, hierarchy and object lifetimes while
the original leaf wheel script admits the same qualified existing-control edits.
Nested Slider value/vertical-scroll synchronization and arrow-button state run
synchronously; clamped unchanged values retain the original no-callback behavior.
The capability ends on protected errors and native unwinding. Arbitrary virtual
input consumers, other input kinds and direct script calls retain ordinary state
admission. Reentry, resize, rendering/collection, movement and top-level visibility
guards remain enabled. Current qualification and input evidence live in
[#126](https://github.com/RovxBot/wowdecomp/issues/126); broad operator acceptance
remains [#96](https://github.com/RovxBot/wowdecomp/issues/96).
Native Slider snapping keeps the original float arithmetic and checks the signed
32-bit quotient before conversion. Range and step changes preflight their entire
clamped span before publishing state or running callbacks, including controls
whose value is not yet set. Invalid operations preserve state and notification
order; ordinary callback, resize and owner guards still apply. Numeric safety is
tracked in [#113](https://github.com/RovxBot/wowdecomp/issues/113), separately from
[press/drag admission #130](https://github.com/RovxBot/wowdecomp/issues/130).
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

## CPU model/material continuation — 2026-10-27

Implementation now selects ShaderEffect's original fixed material/CPU vertex
path consistently with the Model2 cache. Native model passes clear inherited UI
shader bindings inside their existing render-state scope. The existing native
buffer owner submits checked PNT/PNT2 position, normal, color and texture0 arrays
through its private VAO. The device delivers fixed material, four original lights,
linear fog, six original texture operations and texture0 matrices/sphere mapping.
Other texture units, projected texgen and mixed fixed/ARB pipelines remain
explicitly unavailable. UI/FFX ARB shader objects are independent of Model2's
selection; no advertised limit or palette bounds check changes.

A focused authored regression runs original `CM2SceneRender::DrawBatch`, CPU
weighted transforms using bone 142, rebased indices and fixed material paint.
It checks pixels at the translated and original positions and unchanged vertex
constant storage with a 143-bone section. It is separate from game-data evidence.
Implementation `7fc76f952405c3e672bfd1c3f64eb7198cbb4609` and matching-tree
Actions checkout `e44fe5394d3f8d1c3faf832fed67db179979c5f9` pass four workflows/six
jobs, including release/strict startup, both Linux GL configurations (38 CTest
checks each) and full signatures-enabled Windows compilation. The local source
suite runs 61 tests successfully with one existing skip. Downloaded release and strict
fixtures each pass three authored CPU pixel cycles and three separate original
font/UI-shader cycles on RX 7900 XTX / Mesa 26.2.5 / SDL X11, with empty stderr.
No client or dependency compilation ran locally.

One release and three untraced strict ASan/UBSan/leak original-data executions
retain the full ordered creation digest and login activation, then exit 1 at
`Native legacy render state unavailable`, with zero workers and no sanitizer
finding. Separate release GDB identifies state 22, `Texture1`, during original
`CM2SceneRender::DrawBatch`. A material requests a second texture binding while
the device qualifies only one unit. Continue bounded original material/multipass
handling under that limit; do not silently omit the binding or advertise an
unqualified capability. Actual original model/FFX/login frames remain incomplete.

The [durable validation](cpu-model-material-validation.json) retains exact source,
artifact hashes/types/notices, all five PR #89 post-merge checks, initial fixture
null-scene UBSan and uncleared-depth pixel failures, the ARB texgen regression,
and superseded workflow cancellations. The missing-manifest wrapper invocation
before its download completed launched no client and is not runtime evidence.
Original-data agreements remain unaccepted. No physical-input, authentication,
character-selection or gameplay result is established. Signatures, read-only data,
interactive sign-in and server/Warden policy remain unchanged.

## Model paint bounds — 2026-10-27

[PR #89](https://github.com/RovxBot/wowdecomp/pull/89) permits `GetRect` to copy
already-resolved coordinates during original model paint. Its narrow check
requires the XML owner thread, installed layout and live frame. It performs no
anchor resolution or geometry mutation. Setters, structure edits, resize,
creation and destruction retain their render-time guards; unresolved rectangles
still return failure without changing the output.

The first implementation `979028562977221b7f2f8b290e8084346f781f46` passed all
five hosted workflows, but original-data execution exposed a global-buffer-overflow
in `CM2SceneRender::DrawBatch`. It wrote the GPU bone palette before Unlock's
range check. The release run later received SIGBUS during device teardown; the
single strict run halted at the overflow. Neither completed cleanup or qualified
the reached original-data paint path. Their reports and separate debugger traces
remain in [the evidence record](model-paint-validation.json).

Corrected implementation `d79ccf96f886ff2cbe73b44e5749519388c7855d`, matching-tree
Actions checkout `d42ea17efc20591ac70eb3f44975607f4dab7139`, adds a native checked
write-span lock to the existing device. All three normal/instanced/shadow model
palette writers use it before exposing shadow storage, with wide count arithmetic.
It enforces both the qualified 94-register ceiling and advertised cap. Original
signatures, Unlock behavior and Windows paths remain intact.

One release and three untraced strict ASan/UBSan/leak original-data runs retain
the full ordered creation digest and login activation, then stop visibly at
`Native shader constant write range`. All exit 1 with zero workers and no
sanitizer finding. Separate release GDB observes target 0, index 31, count 429
(143 bones). The cache selects CPU model loading, but shader-enabled material
dispatch still enters the GPU palette path. Connect the original CPU rendering/
material path next, retaining the preflight and device limits; loading success
does not establish CPU rendering. Protected Lua state-mutation errors also occur
in the separate initial diagnostic trace and remain unqualified follow-up paths.

All five corrected workflows/ten jobs pass, including Linux release/strict startup,
both GL configurations, actual ARM64 macOS and Windows layout fixtures and full
Windows x64 compilation. Focused contracts check cached reads and retained
mutation/thread guards, valid first/last register spans and rejected oversized/
overflowing palettes without changing storage. All PR #88 post-merge checks pass
separately. The pinned Apple palette loops were reviewed with no imports.
No completed original login/model frame, physical input, authentication or
gameplay is established. Original signatures, unaccepted original agreement
state, read-only data, Actions-only compilation and server/sign-in policy remain.

## FFX depth-target continuation — 2026-10-27

[PR #88](https://github.com/RovxBot/wowdecomp/pull/88) admits original bit 8 on
render-target creation and requires retained implicit depth when that sampler
flag is present. Original OpenGL interprets it as a depth-24 attachment; the
qualified native renderable owner already allocates and retains that storage.
Sampled textures still reject the flag. Attachment/sampler ownership, feedback,
callbacks and teardown remain enforced. No backend, shader-register or texture-unit
capability was added. The [pinned Apple review](APPLE_STARTUP_REVIEW.md#ffx-render-target-continuation--2026-10-27)
records the inspected ownership paths and no imports.

At implementation `e08c076f3ecaa6778b7e249e2c91b054bdc159c2`, matching-tree Actions
checkout `ad7215c0358de7f3d4bf60b9b9d64bb3298e1a6f`, one release and three untraced
strict ASan/UBSan/leak original-data runs retain the full ordered creation digest
and original login activation. They pass FFX allocation, then stop at
`Finish Frame rendering before layout/region access`, exit 1 and close with zero
workers and no sanitizer finding. Separate release GDB inspection identifies
`CSimpleModel::RenderModel` calling `CLayoutFrame::GetRect` during batch drawing.
This getter copies cached resolved coordinates; retain layout mutation and
lifetime guards when connecting that read.

All four implementation workflows/six jobs pass, including full Windows x64
compilation and both Linux GL configurations. Downloaded GPU release and strict
fixtures each pass three depth-attachment, invalid-sampler, callback/PCT/index and
original-font/UI-shader pixel cycles. Those authored draws do not establish a
completed original login/model frame. Exact identities, hashes, runtime reports,
debugger scope and all PR #87 post-merge passes are retained in the
[depth-target evidence](ffx-depth-target-validation.json). Read-only installations,
Actions-only compilation, required signatures, 94-register/one-texture-unit limits,
unaccepted original agreement state and interactive sign-in remain unchanged.
Authentication and gameplay remain unobserved.

## Original login activation — 2026-10-27

[PR #87](https://github.com/RovxBot/wowdecomp/pull/87) connects the five original
agreement status/notice/explicit-callback groups through the existing Glue/CVar
owner. Initialization reads saved values; it never invokes acceptance. Number/nil
return contracts and the original `-2` to `-1` reread transition remain. Authored
fixtures exercise callback transitions and owner recreation in a VM with no game
data or agreement files. Retained closures reject calls after unregistration.

The original no-selected-realm `GetServerName` result reads the saved realm-name
preference and returns no PvP/RP flags plus the original unavailable result. It
does not infer live realm identity, availability or an endpoint. `IsTrialAccount`
retains its original no-login conversion-preference fallback. Authenticated
metadata fails until the real native session owner is connected. Optional void
`PlaySound`/`StopAllSFX` calls report unavailable audio, creating no sound kit IDs,
handles, retries or finished events. Playback APIs that would unconditionally
return success remain unselected.

At implementation `31f52f2439ce657bfb1f0d6d4676940b692206bb`, matching-tree Actions
checkout `1efa8e1e36ab374641ba930a9266dc467d9c7295`, one release and three untraced
strict ASan/UBSan/leak original-data runs authenticate 28,948 archive records and
60 GlueXML visits, match the full creation digest and print
`startup-stage=glue-activated screen=login`. First ModelFFX painting then stops at
`Unavailable legacy native texture contract`. All four exit 1 with zero workers
and no sanitizer finding. Fresh original-data agreement settings remain absent
or zero, retaining the original unaccepted default. No completed login/model frame,
physical-input acceptance, authentication or gameplay is established.

Separate release GDB inspection places the request in `CSimpleModelFFX::Render`
through `FFX::Start`, `FFX::AllocTex` and `FFX::CreateTex`: target 0, 1024×1024,
depth 0, matching ARGB8888 format 2 and flags `0x381`. The original OpenGL
`IDeviceSetRenderTargetFBO` interprets bit 8 as an implicit depth attachment for
the color target. The qualified native target service already owns depth-24
renderbuffers; the legacy flag admission remains the next blocker. Connect that
existing contract before continuing model/material/FFX paint, retaining the real
94-register/one-texture-unit limits.

All three final implementation workflows/four jobs pass: release/strict startup
contracts, source audit and full signatures-enabled Windows x64 compilation.
All PR #86 post-merge checks pass separately. Earlier manual builds, cancelled
checks and intermediate `GetServerName`/`PlaySound` stops remain in the
[activation validation](login-activation-validation.json), alongside downloaded
artifact hashes, final reports and separate debugger/source-inspection evidence.
Inspected original interface bytes stay outside Git. There are no new imports,
local compilation, original-data agreement acceptance or server-policy changes.

## Native model instance continuation — 2026-10-27

[PR #86](https://github.com/RovxBot/wowdecomp/pull/86) selects the existing original
CPU skinning path when the real device cannot fit the smallest GPU profile.
Original GPU selection needs 21 bones; 94 vertex registers leave 20 after the
32-register reservation. UI/FFX shader support alone does not establish GPU
skinning. The owner resets retained cache flags on recreation, retains the actual
94-register/one-texture-unit limits and enables no animation worker.

Real model initialization exposed three native defects beyond component fixtures:
misaligned `M2ModelLight` construction, signed overflow in the particle seed shift,
then a 165-byte/three-object empty sequence-table leak. Native instance arrays,
pointer arrays, ribbon storage and individual polymorphic emitters now reserve
checked aligned offsets within the original single allocation, retaining original
32-bit byte bounds. An explicit native base pointer owns storage even if the
first bone array is empty. Both emitter constructors compose the seed with
unsigned shifting. Inline-only models allocate no empty sequence table; destructor
ownership follows the pointer rather than capacity. Windows paths retain their
original selection and layout.

At implementation `d1ef0d61737346ad2c28871e6ff1946901852f1d`, matching-tree Actions
checkout `1afc668e76ef3c5ef947868b33ff65b18af00775`, one release and three untraced
strict ASan/UBSan/leak original-data runs authenticate 28,948 records and 60 GlueXML
visits, match the full creation digest and pass original model initialization.
They then stop at `AccountLogin.lua:263` calling missing `EULAAccepted`, exit 1 and
close with zero workers and no sanitizer finding. Successful activation and
original model/material/FFX paint are still unobserved.

Three final implementation workflows/four jobs pass: release/strict startup
contracts, source audit and full signatures-enabled Windows x64 compilation.
Authored regressions cover the original skin-profile boundaries and actual CPU
selection, typed placement after an odd-sized array, allocation overflow,
particle construction/copy and zero-capacity sequence-owner destruction.
Local source audits pass 61 tests with one skip. All three PR #85 post-merge
workflows pass separately. Continue the reached original agreement/account
activation services, then model/material/FFX paint.

The [model-instance validation record](model-instance-validation.json) preserves
all hosted identities, final artifacts/hashes and original-data reports, the
earlier leak report and the prior turn's alignment/shift observations. Temporary
raw files for those first two findings were lost across the session restart;
the record claims no unavailable raw-report hashes. Earlier hosted fixture passes
are kept separate from their failing original-data executions. No imports,
local compilation, original-data agreement acceptance, authentication, server
policy change or gameplay result is included.

## Glue activation continuation — 2026-10-26

[PR #85](https://github.com/RovxBot/wowdecomp/pull/85) connects the reached original
activation wrappers through the existing script/CVar/Event/file owners.
`ShowChangedOptionWarnings`, `GetChangedOptionWarnings` and acknowledgement retain
their original number/nil and result-count contracts. Warning strings are owned
until acknowledgement/close, bounded at 256 and tested through Lua collection.
`IsSystemSupported` retains its original SSE-only meaning on Linux x86-64.
`SetCurrentScreen` keeps the original 64-byte state and actual
`AsyncFileReadWaitAll` barrier. Character screens reject transition until their
real OnHide/Idle lifecycle is available.

`SetGamma` and `SetTerrainMip` now admit refreshes of the unchanged preference;
changes fail explicitly while display/world application remains unavailable.
They retain the original slider mappings and reject unsafe numeric conversions.
These are bounded refresh contracts, not working graphics controls. Stored CVars
remain preferences, not applied hardware state. Optional void Glue music/ambience
calls report unavailable playback and create no handles, poll retries or finished
events. Movies and actual audio remain unavailable.

At implementation `e7ca007378535be03229461006b0c01b7635353b`, matching-tree Actions
checkout `59724c1ab554567e9f9969ead05ec294a2c9bc2a`, one release and three untraced
strict ASan/UBSan/leak original-data executions authenticate 28,948 records and 60
GlueXML visits and match the full post-creation digest. They pass the former
`SetGamma` stop, then fail at the real model-read barrier while initializing
`interface\\glues\\models\\ui_mainmenu_northrend\\ui_mainmenu_northrend.m2`.
Each exits 1 with zero workers and no sanitizer finding. The new successful
activation marker is not reached; no login/model paint is established.

Separate release GDB/source/assembly inspection reaches `CM2Shared::Initialize`
after model relocation and observes `maxBones=20` for profile counts 4, 1 and 1.
The truthful 94-register budget yields `(94 - 32) / 3 = 20`; the smallest original
skin profile requires 21. Continue by selecting the existing original CPU skinning
path when GPU skinning cannot fit, preserving 94 registers/one texture unit,
then observe model/material/FFX initialization and paint.

All three implementation workflows/four jobs pass: release/strict authored startup
contracts, source audit and signatures-enabled full Windows compilation. The
Actions checkout tree equals the implementation tree. All 14 post-merge PR #84
workflows pass separately. Local source audits pass 61 tests with one skip.
The [activation validation record](glue-activation-validation.json) preserves
artifacts, binary/notice hashes, exact hosted identities, all four original-data
reports and separate debugger evidence. Earlier superseded runs are cancellations,
not passing implementation checks. No imports, local compilation, agreement
acceptance, server-policy change, authentication or gameplay result is included.

## Original Glue service continuation — 2026-10-26

The integrated target selects the reached original Glue script services through
its existing FrameScript, CVar, file, Frame and Event owners. Glue `GetBuildInfo`
returns the five original results: localized `VERSION` and `RELEASE_BUILD`, then
`GAME_VERSION`, `GAME_BUILD_STRING` and `GAME_BUILD_DATE`. The separate world API
keeps its four-result contract. The saved-account/list/token queries and setters,
CVar value/default/bounds queries and setter retain original argument handling,
read-only failures and number/nil/boolean distinctions. Unknown CVars still fail;
no synthetic option values are generated.

Installed expansion is derived from successfully mounted `lichking.MPQ` or
`expansion.MPQ`, in that precedence order. Glue returns the original installed
level plus one; it does not claim an account entitlement. The target still selects
enUS and complete local retail packages. Streaming/trial queries reflect that
actual file adapter. Settings stay in the external CVar owner. Retained Lua
service closures reject access after unregistration or CVar shutdown. Frames close
before Glue/character function unregistration, which clears borrowed character
model-frame bindings before model/service teardown.

Original character-selection/customization frame setters use the canonical live
object lookup and actual ModelFFX objects. Model `OnUpdateModel`/`OnAnimFinished`
and MovieFrame's three movie handlers are admitted only on their corresponding
XML parent types. The inactive original MovieFrame identity, pooled lifecycle,
script callbacks and subtitle setting are selected. **Native movie playback is
unavailable:** `StartMovie` returns failure/nil with an explicit diagnostic and
never creates a decoder, sound resource, video frame or finished event. There is
no FMOD or Windows decoder implementation linked by this surface.

The original FontString justification and vertex/text-color functions now feed
the existing Gxu text layout/cache. Color changes retain original conversion,
alpha omission and font-override semantics. Tooltip font assignment, click registration, keyboard/mouse/wheel
enablement and frame-level APIs use the existing scoped load-state admission.
Normal traversal, rendering, callback destruction and canonical-object guards
remain enabled. The source continues to use original Lua/XML bytes; these changes
do not substitute an authored login screen.

The selected original video-option queries read the installed Gx format and
capabilities. Only its actual 24-color-bit/24-depth-bit, single-sample drawable is
listed as a multisample format. Monitor enumeration remains unavailable: the
original resolution fallback list is used and refresh enumeration returns no
rates. Original graphics/world/component preferences and original gamma/terrain-mip
queries are registered; option application and device
restart are unselected. Queries retain the real 94-register/one-TMU device limits.

Original sound preferences and the eight game/chat driver queries are selected.
There is no native audio backend. The original no-system contract returns zero
drivers and failed name reads, exposed as empty strings by the original wrappers.
Startup reports `native-unavailable=audio-backend`. Saved/default option values
are preferences, not active hardware settings; backend callbacks, playback,
restart and capture are unselected. No devices, playback success or movie-finished
events are fabricated. There are no new dependencies or donor imports.

The original security-matrix templates request `frameLevel="200"`. The existing
layout owner now carries an immutable, bounded frame-level budget: historical
component probes retain 128; integrated Glue selects 512. Original negative-to-zero
clamping and maximum positive increment of 128 remain. Descendant changes are
validated before list mutation. Parent depth stays 64, anchor paths 2,048 and layout
population 8,192. A level budget is distinct from any of those graph bounds.

The existing startup fixture covers five-result/localized build info, original
saved-account/CVar behavior, closed closures, installed expansion state, actual
character frame binding, model XML callback retention/dispatch, inactive movie
failure and load-callback state edits. It checks original level clamping and
rollback when a child would exceed 512. Native layout checks cover invalid budgets,
explicit selection and restoration of the default across owner recreation. The
Frame hierarchy fixture checks that justification moves actual glyph pixels,
color changes tint them and both restore the original image. It rejects invalid
color components, forged objects and edits during batch collection. These are
authored contracts, not observed original model rendering, movie playback or
authentication.

At implementation `cffe0665ac258d3439ed83014eeca3823c881677`, downloaded
original-data startup authenticates 28,948 archive records and 60 GlueXML visits,
completes ordered creation and prints
`startup-stage=glue-created post-creation-digest=matched`. Activation then fails at
`*:OnValueChanged` calling the unregistered `SetGamma`; device display-gamma
application remains explicitly unavailable. There is no original login/model
paint, authentication, world entry or new physical-input acceptance.
One release and three strict ASan/UBSan/leak original-data runs reproduce that
stop, exit 1 and close with zero workers, without sanitizer findings. Separate
authored startup checks pass one release/three strict executions with empty stderr;
release/strict GPU hierarchy fixtures each pass three cycles with empty stderr
and zero workers. Their original-resource glyph checks remain authored composition.

Five selected hosted workflows/ten jobs pass at their actual checkouts: startup
and source audit at `cffe0665`, Linux GL (38 checks per configuration) at `9bda8c1d`,
native layout (15 checks in each of four configurations, including ARM64 macOS)
at `290e804f`, and full signatures-enabled Windows x64 at `a13357f7`. The record
contains the native source-group and Windows-selected-token input reuse proofs;
earlier runs are not labelled final-head checks. Local source audits pass 61 tests
with one skip. Earlier failed/cancelled runs, the authored invalid-XML/null-tree
UBSan finding, GDB-only diagnostics and historical X11 failure remain separate.

The next batch identified at that source was original activation state and services: changed-option warning
state, the original SSE-only `IsSystemSupported` query, current-screen/lifecycle
tracking, Glue audio with honest unavailable-backend outcomes, and reached option
application. A diagnostic-only release run at `9132183f` records those protected
Lua failures separately; its then-missing graphics preferences are now registered.
Retain real option/device failure behavior rather than treating stored preferences
as applied hardware state. Continue model/material/FFX paint after activation,
then transport/authentication and the user's interactive sign-in.

The [Glue validation record](integrated-glue-services-validation.json) preserves
exact build identities, downloaded reports and earlier failures. The historical
model-factory record below retains its former GetBuildInfo stop and draft status;
those observations are not current runtime evidence for this continuation.

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
