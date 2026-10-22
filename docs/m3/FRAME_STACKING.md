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
child-level shifts and stationary focus refresh. Repeated unoccluded raises do
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

Qualification is pending hosted compilation and exact-source GPU execution.
Title/layout move/resize, pooled Lua/XML/language/template factories, complete
widget behavior, physical mouse/IME/edit-box/candidate acceptance, full GlueXML/
login, transport and gameplay remain open. M3 is incomplete. Preserve original
signatures, historical evidence, server revision discrepancies and interactive
user sign-in.
