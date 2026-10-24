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
source/build/runtime identities will be recorded before merge. EditBox and other
widget consumers, complete signed ordered GlueXML/login execution, transport,
authentication and observed gameplay remain open. The clean client remains
read-only, server revision discrepancies remain explicit, and user sign-in stays
interactive. M3 remains incomplete.
