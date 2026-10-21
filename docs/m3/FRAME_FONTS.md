# Original Frame font factories

This package follows the qualified [layout owners](LAYOUT.md) and [Gxu](LEGACY_GXU.md).
It selects the product's original `CScriptObject`, `CSimpleFont`, `CSimpleFontable`,
font attributes, font Lua methods and global `CreateFont` into the native ABI.
The original scalar XML/font/color helpers are shared with Windows by moving
bodies into explicit translation units. No donor, new dependency or binary
verification claim is introduced; original Windows paths retain their bodies.

`Frame::Native::Fonts` pins layout/XML/VM and Gxu. Close external script objects,
fontables, text blocks and font handles before this owner, then Gxu, layout, XML,
VM, device/context and file queue. Named factory pointers and public fields are
borrowed owner-thread state. `CheckCanClose` rejects external consumers and
standalone fonts; the owner clears named fonts and their metatable. Existing
registered fontable lists and original override/dirty masks remain authoritative.
Do not mutate registration lists or destroy consumers during update callbacks.

Native script dispatch validates live `FrameScript_Object` pointers and canonical
Lua tables before virtual calls. Fonts derive directly from that base, not from
`CScriptObject`; the original opaque dispatcher result is interpreted by its
concrete caller. Named script lookup uses a checked derived type. Destroyed or
forged Lua tables fail visibly, including tables containing a copied valid pointer.
Native parent/name bounds, finite numeric checks, initialized shadow offsets,
512 live font/64-level inheritance bounds and owner leases protect these callers.
Unavailable font sources return the original SetFont failure result. No native
file ticket is exposed as a legacy async object.

The Linux GL presets now include the real layout/XML/FrameScript/product-Lua
graph and exact source audits. `NativeFrameFontsCheck` runs three owner cycles
with authored font inheritance, overrides, copying, XML dimensions/color/shadow,
Lua methods/global CreateFont, names, table invalidation, wrong-thread rejection,
callback lifetime and Gxu text rendering. Optional original-data mode authenticates
GlueXML's interface signature, loads actual GlueFonts definitions into the original
font factories, compares the signed content again and renders via Services/Gxu.
That fixture does not perform the complete FrameXML factory traversal or replace
`CGlueMgr::Resume`'s required post-creation digest check.

All compilation runs in [M3 GL Actions](../../.github/workflows/m3-gl.yml), including
strict ASan/UBSan/leak checks. Full Windows x64 compilation remains the separate
regression check. Hosted and original-data results are pending qualification.

Actual regions/font strings/root batching, SDL resize invalidation, disk/atlas
images, complete FrameXML/GlueXML execution and original login rendering remain
open. This package's rendered text does not establish an original login screen,
physical-input acceptance, macOS graphics, Windows runtime or gameplay. M3 remains
incomplete; preserve signatures, server revision discrepancies and interactive
user sign-in.
