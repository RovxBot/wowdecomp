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
