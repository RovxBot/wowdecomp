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

Qualification is pending hosted compilation and exact-source GPU execution.
