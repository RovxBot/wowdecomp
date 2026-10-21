# SDL IME delivery to original Frame queues

This extends the qualified [keyboard/text/focus adapter](FRAME_KEYBOARD.md) with
original `CSimpleTop::OnIme` stratum/priority dispatch and typed `OnLayerIme`
consumers. It selects the original empty base handler. No donor or dependency
changes; the original Windows-selected bodies remain unchanged. All client and
dependency compilation runs in GitHub Actions.

The host pumps SDL and calls `OsQueueGet`, then immediately calls
`Frame::Native::DispatchInputEvent` before dequeuing again. OsIME getters borrow
the dequeued event's retained composition/result/candidate snapshot. Consumers
must not pump or drain the queue. Native events retain original action/flag
values and event ID `0x40060068`, with explicit UTF-8 code page 65001. Actions
1 through 6 cover composition start/change/end and candidate open/change/close.
Raw actions, flags and reserved fields are checked before any consumer runs.
Action zero (language mode) remains unsupported because SDL does not supply the
original Windows input-language selector. Unknown mouse and other events remain
owned by the host; selected unhandled IME events can propagate to other consumers.

Composition updates, committed results, scalar clause positions and candidate
selection retain the existing Os snapshots. IME commits are not also emitted as
character events. Focus loss now cancels candidate-only sessions as well as
nonempty composition text. This is event delivery, not an edit-box, candidate UI,
language-selection or physical IME qualification.

Dispatch uses the same owner/current-context and layout traversal guards as
keyboard input. Hierarchy mutation, destruction, nested input and root update/
render reject during a consumer callback. Deferred deletion is allowed and occurs
after dispatch. Exceptions propagate and unwind the guard. Hide, disable and
destruction retire consumer queue entries. Close consumers/root before renderer,
font factories, Gxu/layout/XML/VM, device/context and file queue.

`NativeFrameHierarchyCheck --ime-events` repeats three full owner cycles. It
injects composition updates, a supplementary Unicode scalar, candidate open/
change/close, commit, cancellation and focus loss through actual SDL pumping,
Os dequeue and original root queues. It checks every retained snapshot after
pumping all events, priority/stratum order, handled propagation, malformed/foreign
input rejection, callback guards/failure recovery and deferred destruction, then
the existing hierarchy rendering and repeat pixels. The SDL-only fixture also
checks candidate-only focus cancellation on its native platform matrices.

After downloading and verifying a passing Actions artifact, execute without
compiling locally:

```sh
python3 Tools/Porting/run_native_gl.py --artifact /tmp/verified-frame-ime \
  --frame-ime --data /mnt/games/3.3.5aClean/Data \
  --settings /tmp/ime-settings --cache /tmp/ime-cache \
  --output /tmp/ime-runtime.json --snapshot /tmp/ime.ppm
```

Use `--sanitizers` with the strict artifact and separate user roots. The installation
stays read-only and outside Git/hosted CI. Optional original-data mode retains
original-key archive authentication and the GlueFonts subset's before/after
signature checks. Complete signed ordered GlueXML creation and the full
post-creation digest gate, pooled Lua/XML/language/template factories, mouse/
occlusion/drag, edit boxes and actual login rendering remain open, followed by
transport/authentication and observed GrimGuzzler gameplay. M3 stays incomplete.
No new physical-input, Windows runtime, macOS graphics or server acceptance is
claimed. Preserve signatures, server revision discrepancies and interactive sign-in.

## Qualification

[PR #55](https://github.com/RovxBot/wowdecomp/pull/55) is qualified at
`77177a99bcd2ef524c42bfb9bdeba1081cd07f61`, merged as
`da722de8093aba62baa59805c11e35c7e6b857b9`. The [durable record](frame-ime-validation.json)
retains all thirteen passing PR workflows, 37 CTest checks per Linux release/strict
ASan/UBSan/leak configuration and signature-enabled full Windows x64 compilation.
Hosted checkout `5629a4093141fe46d9e140c4793620bac0066828` has the same tree as
implementation and merge. Three archive digests, all 51 packaged executable
hashes/types and notices verified. The candidate-only focus cancellation fixture
also passed in the SDL-only actual ARM64 macOS, Windows and three Linux matrix
configurations; that does not establish Frame graphics on those other platforms.

Downloaded RX 7900 XTX / Mesa 26.2.5 / SDL X11 release and strict sanitizer packages
each passed three authored and three original-data IME cycles, plus authored/
original keyboard, window and hierarchy checks and region/batch/GlueFonts/image/
Gxu regressions. All 26 executions exited zero with empty stderr and zero workers.
All four target/transform/buffer/texture fixtures passed three cycles per mode.
Each IME cycle checked eight ordered retained composition/candidate/commit snapshots,
plus cancellation, focus loss, malformed input, dispatch guards and recovery.
These are authored SDL injections, separate from physical IME/language acceptance.

Original-key signaturefile authentication observed 28,948 records; selective
registered-member MD5 checks stayed enabled. Original-data hierarchy/input cycles
authenticated GlueXML before and after constructing eighteen GlueFonts definitions.
The hierarchy captures match the prior qualified baseline and each other, SHA-256
`55b0e2c731d888293851e66a21b2b7eccaa29405097fb4b6b1e65588f60c93cf`.
The release IME capture was inspected: authored white text with original Friz
Quadrata/UI shaders, 131 ink pixels. Loading-bar captures also match prior evidence.
This is authored composition, not edit-box/candidate UI or original login rendering.

Local source/launcher checks: 56 passed, one CMake availability skip; 269 original
Windows function bodies and extracted screen projection match; whitespace passed.
No client/dependency compiled locally. No failed hosted/runtime attempt or detector
suppression was needed for this implementation. M3 remains open with the factory,
mouse, actual edit-box/GlueXML/login and transport/gameplay gates above.
