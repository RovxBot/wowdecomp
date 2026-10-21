# SDL keyboard, committed text and focus delivery

This native prerequisite extends the qualified [Frame hierarchy](FRAME_HIERARCHY.md)
and [SDL size path](FRAME_WINDOW.md). `DispatchInputEvent` accepts dequeued Os key,
committed Unicode scalar and focus events on the current Gx context/root owner.
The host owns queue pumping and all unsupported events. The result distinguishes
unsupported events, selected but unhandled events and events consumed by a Frame.
The existing size dispatcher remains separate. No dependency or donor changes.

Original `CSimpleTop` stratum/priority queues and typed virtual consumers handle
key-down, captured key-up/repeat and characters. Native capture storage covers
`KEY_LAST`, including the two native Super keys beyond the original 787 slots.
Raw native event indices and keys are bounded before shifts or array access.
Original six Shift/Control/Alt modifier bits, event IDs, repeat counts and Os event
timestamps are retained. Super keys have their own bounded key values and do not
alias those modifier bits. A repeated down event routes to the captured consumer;
uncaptured repeats remain unhandled. Duplicate modifier downs are suppressed.
The original PrintScreen release fallback still dispatches a down and retires its
capture. Native priority nodes use matching C++ allocation/deallocation.

Focus delivery runs the original callback and `OsIMESync`. SDL's window owner
queues held-key releases before focus loss. The adapter also clears residual held
keys, modifiers and captures at the focus boundary. This is committed text and
focus plumbing, not composition/candidate delivery or edit-box integration.
Mouse, hit testing/occlusion, relative motion and drag consumers remain open.

Input dispatch holds a layout traversal guard through external callbacks and
virtual consumers. Hierarchy mutation, destruction, root update/render and nested
input delivery reject; consumers may request the qualified deferred deletion
queue. Exceptions propagate and unwind the guard. Queue removal, hide and
recursive destruction retire captures; level/stratum moves preserve the original
capture behavior. Close consumers/root before renderer/font/Gxu/layout/XML/VM/
device/context/queue owners. Native default handlers return the original empty
result; nonempty Lua input handlers reject before stack mutation until pooled
script object/metatable factories are installed. No Lua input execution is claimed.

`NativeFrameHierarchyCheck --keyboard-events` performs three full owner cycles,
including authored SDL key/repeat/text/focus injection through `Window::Pump`,
`OsQueueGet`, the adapter and actual root queues. It checks ASCII, U+00E9 and
U+1F600, priority/stratum order, capture/release, the highest native key, modifiers,
PrintScreen, hide/disable/deletion, unsupported/invalid/foreign input, callback
failure/reentry and deferred deletion, then the existing hierarchy render checks.
It preserves repeat pixels. This is injected input, separate from physical-input
or language/IME acceptance. Optional original-data mode retains original-key
package authentication and the GlueFonts subset's before/after signature checks;
complete signed ordered GlueXML creation and the full post-creation digest gate
remain open. Game data stays read-only and outside hosted CI.

All client/dependency compilation and CTest run in Actions, with strict
ASan/UBSan/leak detection and separate signature-enabled full Windows x64 builds.

## Qualification

[PR #53](https://github.com/RovxBot/wowdecomp/pull/53) is qualified at
`75225260de3b03a17b2f280ab75f377a9f295d42`, merged as
`74eeee2399937ed72da27f308e9d66ba370d8f23`. The
[durable record](frame-keyboard-validation.json) retains all thirteen passing PR
workflows, 36 CTest checks per Linux release/strict ASan/UBSan/leak configuration
and full signature-enabled Windows x64 compilation. Hosted checkout
`c2f06f3b20e3335f976682bb28fc1f709828663c` has the same tree as the implementation
and merge. Three archive digests, all 51 packaged binary hashes/types and notices
verified. This graphics surface is Linux only; prerequisite CPU workflows retain
separate actual ARM64 macOS and native Windows coverage.

Downloaded RX 7900 XTX / Mesa 26.2.5 / SDL X11 release and strict sanitizer
packages each passed three authored and three original-data keyboard cycles,
plus authored/original window and hierarchy, region, retained batch, GlueFonts,
image and Gxu regressions. All 22 executions exited 0 with empty stderr and zero
workers. Target/transform/buffer/texture fixtures each passed three cycles per mode.
Each keyboard cycle observed thirteen key-down callbacks and the three injected
Unicode scalars. Original-key authentication observed 28,948 signaturefile records;
registered-member MD5 policy stayed enabled. Original-data hierarchy/window/
keyboard cycles authenticated GlueXML before/after eighteen GlueFonts definitions.

Keyboard/window/hierarchy release and sanitizer captures match the prior PR #51
baseline, SHA-256
`55b0e2c731d888293851e66a21b2b7eccaa29405097fb4b6b1e65588f60c93cf`.
The 640x480 keyboard capture was inspected: authored white text through original
Friz Quadrata/UI shaders, 131 ink pixels. Loading-bar image-regression hashes
remain unchanged. These are injected input and authored composition with original
resources, separate from physical-input, IME/language or login acceptance.

Local Python: 55 passed, one CMake availability skip. The new launcher test's
missing fake-manifest setup was corrected before hosted submission. All four
modified legacy source/header files retain the same Windows-selected tokens;
whitespace passed. No client/dependency compiled locally. No new physical-input,
Windows runtime, macOS graphics or server acceptance is claimed.

Run a verified Actions package without local compilation:

```sh
python3 Tools/Porting/run_native_gl.py --artifact /tmp/verified-frame-keyboard \
  --frame-keyboard --data /mnt/games/3.3.5aClean/Data \
  --settings /tmp/keyboard-settings --cache /tmp/keyboard-cache \
  --output /tmp/keyboard-runtime.json --snapshot /tmp/keyboard.ppm
```

Use `--sanitizers` with the strict package and separate writable user roots. Original
pooled Lua/XML/language/template factories, mouse/IME/animation/model/texture
consumers, full GlueXML/login, transport/authentication and observed gameplay remain
open. M3 stays incomplete. Preserve original signatures, historical evidence,
server revision discrepancies and interactive user sign-in.
