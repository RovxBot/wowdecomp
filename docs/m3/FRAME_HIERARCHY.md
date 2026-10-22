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
unlinking, with a 64-node bound and level indices at most 128. Draw-layer/strata/level bounds and finite effective
scales reject invalid input. Windows bodies remain selected unchanged. Native
initialization sets `m_isShown` before original constructor parenting reads it.
The original root delegates renderer/font initialization and shutdown to their
existing native owners. Strata/nodes and direct Frames use matching C++ allocation.

Original `m_destroyed` expires temporary attribute permissions; it is not an
object deletion queue. `DestroyLater` is a separate owner-thread queue processed
before the next update. Duplicate requests coalesce, and recursive parent deletion
retires requests for its children. Hierarchy changes/destruction during updates, batch collection or
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

## Qualification

[PR #49](https://github.com/RovxBot/wowdecomp/pull/49) is qualified at
`ac717d347e2226e06a4f36e0a2bd485f396f6ef2`, merged as
`9b01385d6a1bdb11a78905dd9c302d559cbc142d`. The
[durable record](frame-hierarchy-validation.json) retains all thirteen passing
implementation PR workflows, 34 CTest checks per Linux release/strict
ASan/UBSan/leak configuration and full Windows x64 compilation with signatures
enabled. Hosted checkout `960ebca2d4bb7a50a7cd8ddd53f3e44a89f02274` has the same
tree as the implementation and merge. Three archive digests, all 51 packaged
executable hashes/types and notices verified. The graphics surface is Linux only;
prerequisites retain separate actual ARM64 macOS and Windows CPU coverage.

Downloaded exact-source release and strict sanitizer packages each passed three
authored and three original-data hierarchy cycles, and three each of unparented
region, retained batch, GlueFonts, image and Gxu cycles. All fourteen executions
exited 0 with empty stderr and zero workers. All four target/transform/buffer/
texture fixtures also passed three cycles per mode on RX 7900 XTX / Mesa 26.2.5 /
SDL X11. Original-key package authentication observed 28,948 signaturefile records;
selective registered-member MD5 policy stayed enabled. All eighteen original
GlueFonts definitions were constructed and GlueXML authenticated before and after
the subset in every original-data hierarchy cycle.

The actual Frame hierarchy rendered authored white text `AV Frame parent`, with
131 ink pixels per original-data cycle and exact repeat pixels. Release/sanitizer
640x480 captures match SHA-256
`5e980e9638c007449355cd4b32e17f75218cda2bc75e10e9f54dadb7ebbe47b7`;
the release capture was inspected. Loading-bar image-regression hashes still
match prior qualification. This is authored layout with original font/shader
resources, separate from actual login layout and physical-input acceptance.

The earlier fixture expected rejection of a valid level shift after moving its
child to level zero; both configurations compiled and passed 33/34 checks. It now
restores child level one before checking descendant overflow. A subsequent release
passed all 34 checks while strict UBSan caught an invalid enum load in the new
strata preflight. The native API now accepts raw unsigned input and validates it
before enum interpretation; the original Windows enum API is unchanged. Both
failures and superseded/cancelled runs remain recorded. No detector was disabled.
Local Python: 53 passed, one CMake availability skip. Source comparison verified
269 Windows function bodies and the extracted original screen projection;
whitespace passed. No client/dependency compiled locally.

Run a downloaded, manifest-verified package without compiling locally:

```sh
python3 Tools/Porting/run_native_gl.py --artifact /tmp/verified-hierarchy-package \
  --frame-hierarchy --data /mnt/games/3.3.5aClean/Data \
  --settings /tmp/hierarchy-settings --cache /tmp/hierarchy-cache \
  --output /tmp/hierarchy-runtime.json --snapshot /tmp/hierarchy.ppm
```

Use `--sanitizers` and separate writable roots for the sanitizer package. The
installation stays read-only. No new physical-input, Windows runtime, macOS
graphics or server acceptance is claimed. M3 remains open; preserve original
signatures, server revision discrepancies and interactive user sign-in.

The subsequent [pooled allocation package](FRAME_POOLS.md) qualifies original
base Frame and typed FontString pool construction with mixed heap/pool ownership.
The subsequent [bounded Lua methods](FRAME_SCRIPTS.md) qualify region/Frame/title/
FontString metatables and pooled Lua text creation. XML registry/templates, language
and remaining widget allocators stay open.

## Next original callers

1. Use the qualified base Frame/FontString pooled constructors when selecting
   `CSimpleFrame_Scripts`, `FrameXML_CreateFrame` and `LoadXML_String`. Adapt each
   remaining widget allocator/deletion before selecting its factory; direct raw
   pool calls do not establish owner registration or constructor rollback.
2. Extend the qualified bounded Lua methods into original FrameXML registry and
   named-parent XML processing, virtual template retention/inheritance, language
   processing and animation ownership.
   Preserve canonical live Lua objects and borrowed XML nodes through consumers.
3. Select actual Frame texture/image-mode/backdrop consumers against qualified
   Services source lookup, readiness and atlas coordinates. Adapt scrolling and
   root input/occlusion policy as their callers require them.
4. Preserve the qualified [SDL display-size path](FRAME_WINDOW.md) and
   [keyboard/committed-text/focus adapter](FRAME_KEYBOARD.md) while extending
   mouse/IME and input/occlusion delivery. Establish original
   coordinate math before fonts/layout; keep independent high-DPI and physical-input
   acceptance separate from programmatic window-event checks.
5. Execute complete signed ordered GlueXML/FrameXML creation and the full second
   digest gate in `CGlueMgr::Resume`, then render actual login before interactive
   user sign-in. Original model/material callers, transport/authentication and
   observed GrimGuzzler gameplay remain separate requirements.
