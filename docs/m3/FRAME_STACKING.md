# Original top-level Frame occlusion and raising

This extends the [bounded pointer adapter](FRAME_MOUSE.md) with original
`CFrameStrata::FrameOccluded`/`CheckOcclusion`, `CSimpleTop::CompressStrata`/
`RaiseFrame` and `CSimpleFrame::Raise`. Native `SetFrameFlag` exposes only the
original top-level bit (1); other feature flags retain their unavailable paths.
All client/dependency compilation and CTest run in GitHub Actions.

Occlusion follows original stratum/level membership, positive rectangle overlap
and descendant exclusion. Hidden Frames are absent from occluding lists but remain
registered for compression. Failed layout rectangles are skipped in the native
path. Batch rebuilding refreshes original bit 0x10. Mouse-down raises the nearest
top-level ancestor before capture, retaining original priority re-registration,
child-level shifts and stationary focus refresh. Dirty-batch retirement visits
all allocated levels, including old source levels above a compressed active range.
Repeated unoccluded raises do
not move levels. Original sparse compression resumes its scan index after each
gap; this is not dense level renumbering.

A native preflight mirrors that scan and rejects final parent/descendant levels
above 128 before compression mutates any lists. Public raising, compression and
top-level flags require the owner outside traversal. Host button callbacks and
virtual consumers run in separate guarded phases, with raising between them;
resize callbacks have their own guard. Callback deletion/reentry/mutation still
rejects, and deferred deletion remains available. Close consumers/root before
renderer/font/Gxu/layout/XML/VM/device/context/queue owners. Windows-selected
source tokens are unchanged. No donor/dependency changes.

`NativeFrameHierarchyCheck --mouse-events` adds three authored stacking cycles:
click-to-raise and priority/capture, repeated raises, sparse/hidden compression,
nearest top-level ancestor and descendant exclusion, full-level and child-overflow
rejection before list changes, touching/invalid/hidden rectangles, callback
consumption/failure/guards, and red/green original FontString draw-order pixels.
Existing input/hierarchy/resource checks remain enabled. Optional original-data
mode uses the read-only `/mnt/games/3.3.5aClean/Data` reference, original signature
policy and before/after GlueXML verification around the eighteen GlueFonts subset.
The complete signed ordered creation and full post-creation digest gate remain
open. Authored frames/text/pointer events are separate from physical acceptance
and actual login rendering.

Run a verified Actions package with `Tools/Porting/run_native_gl.py --frame-mouse`
as described in [pointer delivery](FRAME_MOUSE.md). Keep all writable settings,
cache, reports and captures outside the reference installation and Git. Use the
strict package with `--sanitizers` and separate user roots.

## Qualification

The [original top-level Frame package](FRAME_STACKING.md) in
[PR #60](https://github.com/RovxBot/wowdecomp/pull/60) is qualified at
`1d675153d9fb6a18f70feb46e5808ceec2749138`, merged as
`2e81a1978a5dc34b82db5ce6457b69950199a415`. The [durable record](frame-stacking-validation.json)
retains all four triggered implementation PR workflows: source audit, Linux GL,
CPU layout matrix and full signature-enabled Windows x64 compilation. Both Linux
GL configurations passed 38 checks; CPU layout also executed on actual ARM64 macOS
and Windows. Hosted checkout `d45d48d3efe6a995cbfdc19e6f1bc154bcb795ee`
has the same source tree as the selected implementation and merge. Three archive
digests, all 51 packaged executable hashes/types and notices verified.

Downloaded RX 7900 XTX / Mesa 26.2.5 / SDL X11 release and strict ASan/UBSan/leak
packages passed 30 input/UI/resource executions with three owner cycles each,
empty stderr and zero workers. Each target/transform/buffer/texture fixture passed
three cycles per mode. All fifteen release/sanitizer capture pairs match; final
original-data hierarchy/input captures retain the prior baseline. The inspected
640x480 original-data mouse capture is authored white text with original Friz
Quadrata/UI shaders, not original login rendering. Intermediate stacking buffers
assert red/green draw-order changes, repeat pixels and restoration; those buffers
are not exported captures.

Original same-stratum positive-rectangle occlusion, descendant exclusion, nearest
top-level ancestor raising, hidden registered level preservation and literal sparse
compression now run in the native adapter. Preflight final raised descendants
before list mutation. Retire dirty batches across every allocated level, including
old source levels above the compressed active range. Keep callback guards on both
sides of internal mouse-down raising and retain resize-callback guards. The first
hosted release failure (37/38, duplicate text-batch ownership after compression)
and superseded runs remain recorded; no ownership/pixel assertion or detector was
disabled. Windows-selected source tokens match the immutable pre-PR base.

Title/layout move/resize, pooled Lua/XML/language/template factories, complete
widget behavior and physical mouse/IME/edit-box/candidate acceptance remain open.
Full signed ordered GlueXML creation and its post-creation digest gate, actual
login rendering, transport/authentication and observed gameplay remain open.
No new physical-input, Windows runtime, macOS graphics or server acceptance.
M3 remains incomplete; preserve signatures, historical identities, server revision
discrepancies and interactive user sign-in.
