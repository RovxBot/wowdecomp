# Original Frame movement and resizing

This extends the qualified [pointer adapter](FRAME_MOUSE.md) and [top-level
stacking](FRAME_STACKING.md) with original `CSimpleTop` move/resize selection,
`CSimpleFrame::DragBy`, original layout flatten/unflatten and a direct C++
`CSimpleTitleRegion` owner. The implementation selects this product's 12340
bodies. No donor or dependency changes; Windows-selected source remains unchanged.
All client/dependency compilation and CTest run in GitHub Actions.

The native API checks live Frames, owner/current-context access, finite positions,
raw unsigned movement reasons and anchor indices before enum interpretation.
Stop an active operation before starting another. Title clicks move the focused
Frame without ordinary widget capture. With layout movement enabled, left Alt
selects movable top-level Frames and left Control selects resizable top-level
Frames using the original quarter-rectangle edge/corner policy. Direct C++ script
reason survives mouse-up until explicit stop; no Lua factory is advertised.

Original geometry uses DDC deltas, effective scale, nine anchor points and existing
resize bounds. Completion flattens to the original closest screen anchor. The
user-placed flag remains set. Stop clears the borrowed movement Frame before
unflatten/resize callbacks. Hide, event disable, destruction, host capture/focus
loss and deferred deletion retire movement. Resize exceptions propagate and
release guards; cancellation continues retiring pointer state after a failure.

External callbacks cannot mutate hierarchy, destroy consumers, start/stop movement
or reenter root update/render/input. Internal geometry runs outside those guarded
callbacks. Resize notifications defer raising until the traversal has completed;
no strata lists are reordered inside resize callbacks. Existing bounded raising
and dirty batch retirement stay enabled. Close consumers/root, then renderer,
font factories, Gxu/layout/XML/VM, device/context and file queue.

`Frame::Native::CreateTitleRegion` returns a borrowed parent-owned region and is
idempotent. It uses direct C++ heap allocation, consistent with the qualified
hierarchy; it does not select the original pooled XML/Lua factory. Layout anchors
and title parent state stay valid until the owning Frame closes.

`NativeFrameHierarchyCheck --mouse-events` adds three movement cycles covering
title hit/body capture, rendered attached-text movement and exact repeats, scale,
all nine edges/corners/center, minimum/maximum bounds, layout modifiers, explicit
script stop, cancellation, consumer retirement, malformed/foreign access, resize
callback guards/failure recovery and deferred destruction. Existing pointer,
stacking, IME, keyboard, hierarchy and resource regressions remain enabled.
These authored events are separate from physical mouse/IME/edit-box acceptance.

After downloading and verifying passing Actions artifacts, run without local
compilation:

```sh
python3 Tools/Porting/run_native_gl.py --artifact /tmp/verified-frame-movement \
  --frame-mouse --data /mnt/games/3.3.5aClean/Data \
  --settings /tmp/movement-settings --cache /tmp/movement-cache \
  --output /tmp/movement-runtime.json --snapshot /tmp/movement.ppm
```

Use `--sanitizers` with the strict package and separate writable roots. Data remains
read-only and outside Git/hosted CI. Original package authentication, selective MD5
and before/after GlueXML signatures around eighteen GlueFonts definitions stay
enabled. Complete signed ordered GlueXML creation and its full post-creation
digest gate remain open, as do pooled factories, other widgets, physical input,
actual login rendering, transport/authentication and observed gameplay. M3 stays
open; preserve server revision discrepancies and interactive user sign-in.

## Qualification

[PR #62](https://github.com/RovxBot/wowdecomp/pull/62) is qualified at
`e0540e9b194d71b26459b1d78c0832b910688de4`, merged as
`3b9f9f820905e04ce38a18f91661acbd6e4de7db`. The [durable record](frame-movement-validation.json)
retains all thirteen passing implementation PR workflows, 38 CTest checks in each
Linux release/strict ASan/UBSan/leak configuration and signature-enabled full
Windows x64 compilation at matching-tree checkout
`534d8eb6e229ba3f63fdb4aae0a19df85ce8765c`. Three archive digests, all 51 packaged
binary hashes/types and notices verified. Graphics remain Linux x86-64; the
prerequisite matrices retain actual ARM64 macOS and Windows CPU/shell execution.

Downloaded RX 7900 XTX / Mesa 26.2.5 / SDL X11 release and strict sanitizer packages
passed 30 input/UI/resource executions, three owner cycles each, with empty stderr
and zero workers. Each target/transform/buffer/texture fixture passed three cycles
per mode. All fifteen final release/sanitizer capture pairs match; final
original-data hierarchy/input captures retain the prior baseline. The inspected
640x480 mouse capture shows authored white text through original Friz Quadrata/UI
shaders. Intermediate title-movement readbacks assert changed attached-text pixels
and exact repeats; those buffers are not exported captures or original login UI.

Every authored/original-data mouse execution passed three title/layout movement,
nine-point geometry, resize-bound, scale, cancellation and callback/lifetime cycles.
Original-key signaturefile authentication observed 28,948 records; selective MD5
checks stayed enabled. Original-data cycles authenticated GlueXML before/after
the eighteen GlueFonts subset. Complete signed ordered creation and the full
post-creation digest gate remain open.

Local Python: 57 passed, one unavailable-CMake skip; Windows-selected source tokens
and whitespace match the immutable pre-PR base. All compilation ran in Actions.
No failed hosted/runtime attempt or detector suppression was needed. Duplicate
push runs were cancelled or completed separately; the thirteen PR runs qualify.
Pooled factories, remaining widgets, physical mouse/IME/edit-box/candidate UI,
full GlueXML/login and transport/gameplay remain open. M3 is incomplete; preserve
signatures, server revision discrepancies and interactive user sign-in.
