# SDL pointer delivery to original Frame queues

This extends the qualified [keyboard](FRAME_KEYBOARD.md) and [IME](FRAME_IME.md)
adapter with original `CSimpleTop` mouse tracking, priority/stratum selection,
enter/leave, captured buttons, wheel and relative callbacks. All compilation and
CTest run in Actions. No donor, dependency or original Windows body changes.
Qualification is pending hosted checks and downloaded-package execution.

`DispatchInputEvent` runs immediately after host `OsQueueGet`, outside root
update/render. Absolute coordinates use logical window pixels, normalized X and
inverted Y, then original `CMouseEvent` DDC conversion. Drawable dimensions do not
scale input. Relative events preserve original raw-delta-to-DDC behavior; wheel
units remain signed multiples of 120. Five SDL buttons preserve original masks,
held-button state, modifiers, event identities and timestamps. Unknown events
remain with the host. Raw native buttons, event indices, enum storage and finite
coordinates are bounded before interpretation or access.

Original hit rectangles/insets, scroll-parent clipping and track/drag thresholds
are selected. Typed C++ consumers receive drag start/stop/drop; empty base Lua
handlers retain their state transitions. Nonempty Lua handlers reject before Lua
stack mutation pending pooled factories. The current direct C++ surface has no
qualified top-level occlusion/raising, title-bar movement or layout move/resize;
mouse-down rejects those paths explicitly. Cursor depth/stereo and general widgets
remain unqualified. This is a bounded pointer adapter, not complete mouse/widget
or original login acceptance.

A traversal guard spans callbacks/virtual consumers. Hierarchy mutation,
destruction, root update/render and nested input reject; deferred deletion is
allowed. Exceptions propagate and release the guard. Hide, disable and destruction
retire queue entries, focus, captures and drag state. Capture/focus loss clears
owned state before cancellation callbacks. Stationary focus refresh runs after
root update traversal completes, with its own input guard. Close consumers/root
before renderer/font/Gxu/layout/XML/VM/device/context/queue owners.

`NativeFrameHierarchyCheck --mouse-events` repeats three owner cycles, exercising
priority/stratum order, hit insets and stationary refresh, captured multi-button
release, typed drag/drop, wheel sign/propagation, relative callbacks, focus/capture
loss, hide/disable/destruction, malformed/foreign input, guards, callback exception
recovery and deferred deletion. Authored SDL motion/button/wheel/relative events
pass through actual pumping and Os dequeue. Existing hierarchy rendering and exact
repeat pixels run afterward. This is injected evidence, separate from physical
mouse, IME/language, edit-box/candidate UI or platform acceptance.

Run a verified Actions package without local compilation:

```sh
python3 Tools/Porting/run_native_gl.py --artifact /tmp/verified-frame-mouse \
  --frame-mouse --data /mnt/games/3.3.5aClean/Data \
  --settings /tmp/mouse-settings --cache /tmp/mouse-cache \
  --output /tmp/mouse-runtime.json --snapshot /tmp/mouse.ppm
```

Use `--sanitizers` with the strict package and separate user roots. The complete
installation remains read-only and outside Git/hosted CI. Original MPQ and interface
signature checks stay enabled. Original-data mode constructs only the qualified
GlueFonts subset with before/after signature checks; complete ordered GlueXML
creation and its post-creation digest gate, pooled Lua/XML/language/template
factories, actual widgets/login rendering, transport and observed GrimGuzzler
world entry remain open. Preserve server revision discrepancies and interactive
user sign-in. M3 is incomplete.
