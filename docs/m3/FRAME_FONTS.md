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
callback lifetime, nonfinite XML inputs and Gxu text rendering. Repeated renders
must match every pixel. The larger authored font retains full-coverage red checks;
the small original outlined font uses visible antialiased glyph coverage at its
unchanged logical size. Optional original-data mode authenticates
GlueXML's interface signature, loads actual GlueFonts definitions into the original
font factories, compares the signed content again and renders via Services/Gxu.
That fixture does not perform the complete FrameXML factory traversal or replace
`CGlueMgr::Resume`'s required post-creation digest check.

All compilation runs in [M3 GL Actions](../../.github/workflows/m3-gl.yml), including
strict ASan/UBSan/leak checks. The new authored movement font executes X, Y and
non-axis-aligned interpreter displacement during size initialization, even for
unhinted glyphs. Native bundled FreeType uses correctly typed movement adapters;
its original Windows callback selection and body/index conversion remain intact.
The strict original-data callback failure and the authored before-fix reproducer
remain failed evidence; no detector is disabled.
Full Windows x64 compilation remains the separate
regression check. The qualification below records the selected implementation.

Actual regions/font strings/root batching, SDL resize invalidation, disk/atlas
images, complete FrameXML/GlueXML execution and original login rendering remain
open. This package's rendered text does not establish an original login screen,
physical-input acceptance, macOS graphics, Windows runtime or gameplay. M3 remains
incomplete; preserve signatures, server revision discrepancies and interactive
user sign-in.

## Qualification

[PR #41](https://github.com/RovxBot/wowdecomp/pull/41) is qualified at
`c87df98ab5b8f05cab034941dece92a78f81d726`, merged as
`829904d849546a224201a885fdb4e1c55ce578e5`. [Durable evidence](frame-font-validation.json)
retains thirteen passing PR workflows, 31 CTest checks in each Linux release/strict
ASan/UBSan/leak configuration and full Windows x64 compilation with signatures
enabled at matching-tree checkout `ba54f09734f163123155e378048cadf3bcfde61b`.
Three archive digests and all 45 packaged binary hashes/types were verified.
The existing prerequisite workflows retain actual ARM64 macOS/Windows execution;
the new graphics/font-factory target remains Linux only.

Downloaded exact-source release and sanitizer binaries each passed three authored
movement/font-factory cycles, three original-data GlueFonts cycles and three
original-data Gxu regression cycles on RX 7900 XTX / Mesa 26.2.5 / SDL X11.
All six executions exited 0 with empty stderr and zero workers at each shutdown.
The original InterfaceKey authenticated GlueXML before/after direct construction
of 18 original font definitions. One selected original small outlined font rendered
77 antialiased red pixels (peak 187/255) at unchanged logical size. Exact repeat
readback and release/sanitizer captures match; the matching diagnostic capture was
inspected. Text/red tint are authored probe inputs, not the original login UI.

The first fixture incorrectly required fully covered pixels in that small font.
The strict original-data sanitizer then exposed an incompatible `TT_Move_Func`
call: its `FT_UInt` index differed from the original movement bodies' `FT_UShort`.
A new authored movement font reproduces the defect in the old verified binary;
native typed adapters preserve the original conversion. Both failures and an
explicitly unqualified recovery run remain in the record. No detector was disabled.
Historical tiny/cache font bytes and Windows interpreter paths remain unchanged.
Local source checks: 47 passed, one platform skip; original moved Windows helper
and interpreter comparisons and whitespace checks passed. No local compilation.

Run the downloaded, hash-verified Actions package without local compilation:

```sh
python3 Tools/Porting/run_native_gl.py --artifact /tmp/verified-font-package \
  --frame-fonts --data /mnt/games/3.3.5aClean/Data \
  --settings /tmp/frame-font-settings --cache /tmp/frame-font-cache \
  --output /tmp/frame-font-runtime.json --snapshot /tmp/frame-font.ppm
```

Use `--sanitizers` with the sanitizer artifact and separate user/output roots.
For the authored movement/font fixture, replace data/settings/cache with
`--font Tools/NativeGx/fonts/moves.ttf`. Preserve the original post-creation
`CGlueMgr::Resume` digest gate when the complete Frame factories are connected.

## Next original callers

1. Select `CScriptRegion`, `CSimpleRegion` and the actual `CSimpleFontString`
   bodies in `CSimpleRender.cpp`. Initialize native `m_maxLines` before preserving
   bitfields, retain font handles until text-block close in `SetFont`, and connect
   `FontStringAttributes.cpp`'s original string overload. Naming now shares the
   qualified VM owner; region parent/animation/layer lists still need adaptation.
2. Connect `CSimpleTop` and actual Frame-object queues to the qualified
   [retained batcher](FRAME_RENDER.md), with deferred consumer destruction and real
   SDL size events. `CSimpleFontString::OnScreenSizeChanged` and original Gxu font
   invalidation must both execute; Services text readback alone does not test them.
3. Adapt `CSimpleTexture::SetTexture`'s disk/atlas `0x17` request, atlas coordinates,
   supported image modes and default filtering before selecting real textures.
   Preserve the local wait-2 and authored-mip contracts; unsupported lookup and
   streaming must remain visible failures.
4. Connect the actual `FrameXML_ProcessFile`/`FrameXML_CreateFrames` traversal,
   virtual templates and ordered Lua execution. The direct GlueFonts fixture is
   a font subset. Preserve the full second digest comparison in `CGlueMgr::Resume`
   before FRAMES_LOADED and render original login before interactive sign-in.
