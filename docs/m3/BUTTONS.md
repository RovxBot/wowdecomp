# Original Button widgets

The native Frame graph now selects original 12340 `CSimpleButton` state, text,
texture, click, hover and Lua/XML consumers. `CreateFrame('Button', ...)` and
`Create_SimpleButton` use the original aligned class pool. Windows keeps its
original factory body, moved alongside the already shared base Frame factory.
No game data or dependencies are added.

Original normal/pushed/disabled textures, highlight layers, font objects and
pressed text offsets use qualified native image/Gxu/retained-batch services.
Button XML aliases use the corresponding FontString/Texture template rules.
Native XML click registration preserves all 64 mask bits; mouse-up uses an
unsigned shift. The original Windows accumulator/shift paths remain unchanged.
One texture may occupy one state/highlight slot. Canonical live Lua tables are
required for secondary texture/font/string arguments, before derived-pointer use.

Button mutation and callbacks belong to the Frame/Gx owner. State transitions
can update region draw-list visibility during input without changing the owning
region/Frame tree. Batch collection/rendering rejects those changes. Lua factory,
parenting, destruction and ownership edits still reject during callbacks; existing
Button text can change without allocating a new region. Click recursion is
suppressed, original pre/click/post ordering and double-click dispatch remain,
and callback state restores on C++ unwinding. Close Buttons before Scripts,
images, renderer, Fonts/Gxu/layout/XML/VM/device/context/queue owners. The Fonts
owner releases registered pool blocks only after every slot is returned.

Native mouse-button delivery pins the selected consumer independently of resize.
Only its original `OnClick` admits existing Frame/FontString/Texture visibility and the already
qualified Slider/scroll/Button state setters across PreClick/OnClick/PostClick.
Hide retires captures and cursor focus before descendant/parent OnHide; the
selected consumer remains alive through PostClick. Nested cursor scripts keep
strict admission, including those called by Enable/Disable. Direct Lua Click,
arbitrary mouse handlers, other-button clicks and double-click callbacks retain
ordinary guards. Movement/top-level visibility is preflighted before edits;
layout, hierarchy, deletion, input reentry, resize, collection/render and owner
guards remain active. [Agreement continuation #128](https://github.com/RovxBot/wowdecomp/issues/128)
tracks hosted and original-data acceptance separately from authored transitions.

The hierarchy fixture checks 101 pooled Buttons, canonical/stale Lua tables,
XML inheritance and aliases, original click handlers, right-up mask retention,
normal/hover/pushed/disabled pixels, pressed/released/outside/locked/cancel states,
click recursion and owner/collection admission. Original-data mode additionally
loads unchanged `GlueButtons.xml`, constructs `GlueButtonTemplateBlue` (the template used by the
original AccountLogin login button), renders an
authored label and checks the GlueXML InterfaceKey digest before and after that
subset. This does not execute the entire ordered GlueXML manifest or its final
CGlueMgr gate. Input is authored injection, separate from physical acceptance.

All client/dependency compilation runs in GitHub Actions. Qualification and exact
source/build/runtime identities are recorded below. EditBox and other
widget consumers, complete signed ordered GlueXML/login execution, transport,
authentication and observed gameplay remain open. The clean client remains
read-only, server revision discrepancies remain explicit, and user sign-in stays
interactive. M3 remains incomplete.

The next login consumers are `CSimpleCheckbox` for remember-account options and
`CSimpleEditBox` for username/password entry. EditBox still needs selected pooled
region allocation, font-string-before-font teardown, native key/focus/clipboard
providers and bounded text/IME/candidate consumers. Its owned font currently uses
raw Storm storage and its original destructor closes the font before the base
Frame closes its strings; adapt both before selecting it. Keep candidate-language
and password-input policy explicit rather than accepting an unavailable service.
Then connect the complete ordered TOC/Include/Script executor and the full second
`CGlueMgr::Resume` digest gate. Transport/authentication follows those UI owners;
packet prerequisites alone do not provide a server connection.

The original Blue button images exposed a separate native BLP boundary: their
256x64 DXT base images are complete, but some thin mip tails contain fewer blocks
than the standard DXT grid. Original Windows tail rendering is not newly qualified.
`DecodeBlp` retains strict all-level decoding by default. Its explicit base-only
mode checks all declared offsets, lengths, chain ordering and storage extents;
unrequested thin DXT tails admit only the observed power-of-two, mip-flagged,
area-sized/minimum-block form. Those tails are never decoded or returned. Native
image Services selects this mode for nearest/linear sampling; mip samplers retain
strict complete authored-chain decoding. No missing pixels or generated mips are
substituted. Authored DXT1/3/5 fixtures compare exact base pixels, retain full-chain
rejection, and reject bad tail sizes, spans, truncation, flags and non-thin storage.
A Frame/image fixture exercises the same selection through Services/Gx. Image
failures now retain their actual CStatus detail at the Frame boundary.

## Qualification

[PR #78](https://github.com/RovxBot/wowdecomp/pull/78) is qualified at
`afb952dc28ce9772b1b5edfa1f19bb4ed2b59e4e`; [durable validation](button-validation.json)
retains all exact source/build identities. Final-source hosted GL release and
strict ASan/UBSan/leak configurations passed 38 CTest checks each. Production
`defccaa8914e737a93df7585b3d556a124c7ec07` passed eight asset CTest checks on each
of Linux release/strict sanitizer, actual ARM64 macOS and native Windows, plus
full signatures-enabled Windows x64 compilation. Only this GL fixture and its
contract document changed afterward; the other 3,889 tracked entries match.
The earlier production checks are not labeled final-head runs. Seven archive
digests, all 67 packaged executable hashes/types and packaged notices verified.

Downloaded RX 7900 XTX / Mesa 26.2.5 / SDL X11 release and strict sanitizer packages
passed sixteen authored/original-data wrapper executions, three owner cycles each,
with empty stderr and zero workers. Original hierarchy, mouse, keyboard, IME and
window runs loaded eighteen original base fonts, 57 original style definitions,
the original inline color script and unchanged `GlueButtonTemplateBlue`. Their
before/after GlueXML InterfaceKey checks passed; original-key signaturefile
verification observed 28,948 records and selective registered-MD5 checks stayed
active. The image/layout/font readiness bound was four frames; every observed
original Button cycle became ready in one. This does not qualify every style.

All eight final capture pairs and five additional original-button pairs match
between release and sanitizer. The eight final captures also match the preceding
upstream qualification. The inspected 640x480 button capture shows the original
blue border/fill and legible yellow outlined Friz text; its PPM SHA-256 is
`72e50e7282e9c5169dad8bcb584ee1585990c11b4684507339fd11ddefc9ff73`.
The label `AV login` and placement are authored. The target/transform/buffer/texture
fixtures passed three cycles per mode alongside image regressions. No actual
AccountLogin screen, physical input or server session is claimed.

Earlier graph/include/class-admission and fixture expectation failures remain in
the record. Clean-data attempts also exposed the wrong initial template file,
short DXT tails, missing font styles and the inline Script consumer. Temporary
pure-Python MPQ inspection and release-only GDB diagnostics were investigative;
they are not native authentication or sanitizer qualification. All final runtime
checks ran without tracing and with the strict detectors unchanged. Local source/
launcher checks: 57 passed, one unavailable-CMake skip; Windows-selected tokens,
original factory extraction and whitespace checks passed. No client/dependency
compiled locally. M3 remains open with the next callers above.
