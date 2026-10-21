# Original Frame parent/root integration

This selects original `CSimpleFrame`, `CSimpleTop`, `CFrameStrataNode` and their
child/region/layer/strata lists against the qualified FontString regions. The
original screen projection moves unchanged into `Services/ScreenProjection.cpp`,
shared with Windows. There is no donor/dependency change. Client and dependency
compilation runs exclusively in GitHub Actions.

The first surface is direct C++ with heap-created Frames/FontStrings, owned by
parents and the root. It does not install the original pooled XML/Lua factories.
Close the root before renderer/font factories, Gxu/layout/XML/VM and device/context.
The root uses existing native host scheduling; legacy Scrn, Event, input/animation,
scrolling and backdrops remain visibly unavailable. Original script metatables,
FrameXML factories and full signed ordered GlueXML/login remain subsequent work.

Owner/thread guards protect native mutations. Parent edges are validated before
unlinking, with a 64-node bound. Draw-layer/strata/level bounds and finite effective
scales reject invalid input. Windows bodies remain selected unchanged. Native
initialization sets `m_isShown` before original constructor parenting reads it.
The original root delegates renderer/font initialization and shutdown to their
existing native owners. Strata/nodes and direct Frames use matching C++ allocation.

Original `m_destroyed` expires temporary attribute permissions; it is not an
object deletion queue. `DestroyLater` is a separate owner-thread queue processed
before the next update. Duplicate requests coalesce, and recursive parent deletion
retires requests for its children. Hierarchy changes/destruction during updates or
resize/draw callbacks are rejected; update callbacks can request deferred deletion.
Native dirty-batch release precedes all root rebuilding, preserving the single Gxu
intrusive batch link when the same block moves to an earlier level/strata/layer.
Original layout queues are processed before batches. Display callback tests are
direct injection; actual SDL size/input delivery is not established here.

`NativeFrameHierarchyCheck` repeats three root/owner cycles. It covers actual
parent/child and region/layer membership, root rendering/repeated pixels,
visibility/layer changes, reparenting and text batch movement, inherited alpha and
scale, temporary attribute cleanup, invalid/foreign access and deferred recursive
deletion. Optional original data authenticates original GlueXML/GlueFonts and
renders authored text through the actual hierarchy. It retains the subset's
second signature comparison; complete original Frame creation and the full
`CGlueMgr::Resume` post-creation digest gate remain open. Game data stays read-only
and outside hosted CI. No original login or gameplay is claimed; M3 remains open.

Qualification is pending hosted and downloaded runtime checks. No new physical
input, Windows runtime, macOS graphics or server acceptance is claimed. Preserve
original signatures, server revision discrepancies and interactive user sign-in.
