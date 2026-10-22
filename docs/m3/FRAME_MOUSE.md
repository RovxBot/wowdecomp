# SDL pointer delivery to original Frame queues

This extends the qualified [keyboard](FRAME_KEYBOARD.md) and [IME](FRAME_IME.md)
adapter with original `CSimpleTop` mouse tracking, priority/stratum selection,
enter/leave, captured buttons, wheel and relative callbacks. All compilation and
CTest run in Actions. No donor, dependency or original Windows body changes.
The selected implementation is qualified below.

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
stack mutation pending pooled factories. The initial direct C++ surface has no
qualified top-level occlusion/raising, title-bar movement or layout move/resize;
that package's mouse-down rejects those paths explicitly. Cursor depth/stereo and
general widgets remain unqualified. This is a bounded pointer adapter, not complete mouse/widget
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

The subsequent [top-level stacking package](FRAME_STACKING.md) qualifies original
occlusion and click raising. Title/layout movement and the remaining factory/widget
gates stay open. The pointer qualification below retains its historical identities.

The subsequent [movement package](FRAME_MOVEMENT.md) qualifies original title/layout
move/resize and direct parent-owned title regions. Pooled factories, complete
widgets and physical input acceptance remain open.

## Qualification

[PR #58](https://github.com/RovxBot/wowdecomp/pull/58) is qualified at
`205d3d5770c3ca756909f9c0de63c7a0f3a294a3`, merged as
`53c259f20cb48db3e5e6a4011074b5d0644ad447`. The [durable record](frame-mouse-validation.json)
retains all thirteen passing implementation PR workflows, 38 CTest checks per Linux
release/strict ASan/UBSan/leak configuration and full signature-enabled Windows
x64 compilation at separate matching-tree checkout
`51970b1b468c9a5f5310a56d742895fe913bf414`. Three archive digests, all 51 packaged
binary hashes/types and notices verified. Native Frame graphics remain Linux only;
prerequisite CPU workflows retain separate actual ARM64 macOS and Windows coverage.

Downloaded RX 7900 XTX / Mesa 26.2.5 / SDL X11 release and strict sanitizer packages
passed 30 authored/original-data mouse/IME/keyboard/window/hierarchy and prior UI/
resource executions, three owner cycles each, with empty stderr and zero workers.
Each of four target/transform/buffer/texture fixtures passed three cycles per mode.
All fifteen release/sanitizer capture pairs match. Original-data mouse/IME/keyboard/
window/hierarchy captures retain the prior baseline SHA-256
`55b0e2c731d888293851e66a21b2b7eccaa29405097fb4b6b1e65588f60c93cf`.
The 640x480 release mouse capture was inspected: authored white text through original
Friz Quadrata/UI shaders, 131 ink pixels. This is not actual login rendering.

Original-key authentication observed 28,948 signaturefile records and selective
registered-member MD5 policy stayed enabled. The hierarchy/input probes authenticated
GlueXML before/after constructing eighteen original GlueFonts definitions. The
complete signed ordered creation and full second digest gate remain open.

The first hosted attempt rejected the new original mouse-event source at the graph
audit before compilation; the explicit allowlist and matching local fixture were
corrected. A local source comparison also caught the exception header outside its
native guard before submission. Failed/superseded attempts remain in the record;
no detector or pixel assertion was disabled. Final local Python: 57 passed, one
unavailable-CMake skip. Windows-selected tokens in all three modified legacy files
and whitespace match. All client/dependency compilation ran in Actions.

No new physical mouse/IME/language, edit-box/candidate UI, independent high-DPI,
Windows runtime, macOS graphics or server acceptance. M3 remains incomplete with
the pooled factory, top-level mouse/widget, full GlueXML/login and transport/gameplay
gates above. Preserve original signatures, server revision discrepancies and
interactive user sign-in.
