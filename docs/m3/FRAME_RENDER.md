# Original Frame render batching

This prerequisite follows the qualified [font factories](FRAME_FONTS.md) and
[image lookup/atlases](IMAGE_SOURCES.md), before selecting actual region/root
consumers. The original `CSimpleRender` batch counting, UI vertex shaders and
mesh/text/callback draw bodies move into `CSimpleBatchRender.cpp`; Windows keeps
those bodies and its original borrowed-resource behavior. No donor or dependency
is added. Native builds select only that file, `CRenderBatch` and a native owner.

`Frame::Native::Renderer` lives inside Gxu/device/context ownership. Close every
batch before the renderer. Native mesh queues snapshot quad positions, UVs,
colors and uint16 indices, duplicate texture handles and retain device-validated
pixel shaders. Native text queues duplicate both Services font and block handles:
the block itself borrows the font. Clear unlinks the original Gxu batch before
closing blocks and fonts. Texture handles keep atlas cells and image Services
alive until clear. Callback parameters remain borrowed; keep their owners alive
through consumption. Public legacy fields remain owner-thread implementation
state, not an alternate unvalidated submission API.

The initial native mesh surface admits four positions, six bounded triangle
indices, a live pixel shader, supported blend modes and one/four vertex colors.
It limits each batch to 32,768 entries. Sorting compares full-width texture and
shader keys, retaining the original effective-alpha grouping. Original grouping
and uint16 split thresholds remain intact. Native counts use bounded heap arrays
instead of stack allocations. Owner-thread checks reject batch mutation and
nested draw during callbacks. Render-state scopes unwind on failure; only writes
acquired by the batch are aborted, retaining stream buffers for a later retry.

A text string's intrusive batch link cannot belong to two batches. Duplicate
queue attempts fail rather than silently removing text from another batch.
Retained text remains the original mutable Services/Gxu object; glyph geometry
is not snapshotted. Concurrent external Gxu/list mutation during consumption is
outside this contract. Actual region/root deferred destruction remains future
work; retained lower-level resources alone do not qualify it.

`NativeFrameRenderCheck` exercises three complete cycles of stack-array expiry,
last-caller texture/shader/font/block close, full-width sorting, repeated pixel
readback, original merge boundaries, nonzero authored atlas cells, thread guards,
callback mutation/reentry and exception recovery. Optional original-data mode
uses unchanged Friz Quadrata, UI shaders and loading-bar BLP in an authored
composition; it is not original Frame objects or a login screen. All compilation
and CTest execution run in [M3 GL Actions](../../.github/workflows/m3-gl.yml),
including strict ASan/UBSan/leak checks and separate full Windows x64 compilation.
Native Frame-object queue entry points fail visibly until their actual class
lifetimes are selected; `NativeQueueText` accepts qualified Services handles.
UBSan vptr checks remain enabled. The selected implementation is qualified below.

After downloading and verifying Actions artifacts, run without local compilation:

```sh
python3 Tools/Porting/run_native_gl.py --artifact /tmp/verified-frame-render \
  --frame-render --data /mnt/games/3.3.5aClean/Data \
  --settings /tmp/frame-render-settings --cache /tmp/frame-render-cache \
  --output /tmp/frame-render.json --snapshot /tmp/frame-render.ppm
```

Use `--sanitizers` with the sanitizer artifact and separate writable roots.
Actual `CScriptRegion`/`CSimpleRegion`/`CSimpleFontString`, parent/animation/layer
lists, `CSimpleTop`, SDL size invalidation, Frame texture/image modes, full ordered
signed GlueXML execution and the second post-creation digest gate remain open.
Transport, interactive user sign-in and observed gameplay follow original login
rendering. M3 remains incomplete; signatures and server revision discrepancies
remain intact. No new physical-input, Windows runtime, macOS graphics or server
acceptance is claimed.

## Qualification

[PR #45](https://github.com/RovxBot/wowdecomp/pull/45) is qualified at
`d09ad319696fa7a05767bcc2af5f810af4378b3d`, merged as
`d4b213b01e9a9d50a538c50f3247614930058517`. The [durable record](frame-render-validation.json)
retains all thirteen passing PR workflows, 32 CTest checks per Linux release/strict
ASan/UBSan/leak configuration and full Windows x64 compilation with signatures
enabled. Hosted checkout `08dbdbc46ee4b5b0100aeaa427facc731d81332c` has the same tree
as the selected implementation and merge. Three archive digests, all 47 packaged
binary hashes/types and notices verified. The existing prerequisite workflows
retain native ARM64 macOS/Windows CPU coverage; this new graphics target is Linux
only. No client or dependency compiled locally.

Downloaded RX 7900 XTX / Mesa 26.2.5 / SDL X11 release and strict sanitizer binaries
each passed three authored batch, three original-data batch, three original
GlueFonts, three image and three Gxu cycles. All ten main executions exited 0
with empty stderr and zero workers at every shutdown. Target, transform, buffer
and texture resource checks each passed three cycles in each mode. Original-key
signaturefile authentication observed 28,948 records, with selective registered
member MD5 policy enabled. The separate GlueFonts regression authenticated the
interface signature and constructed eighteen definitions each cycle.

The inspected authored composition uses original Friz Quadrata, UI shaders and
loading-bar BLP through the actual batcher; it is not original Frame objects or
login layout. It rendered 1,521 red-ink pixels in each original-data cycle, with
exact repeat readback and matching release/sanitizer capture SHA-256
`5d02fd7e51a3aad8961b9c04a06fd8d210b169105dd16379e250cddfd7c46482`.
The separate loading-bar image-regression capture still matches prior qualification.
Two vertex permutations upload; only nonstereo rendering is qualified.

Three earlier fixture failures remain recorded: a link to unselected
`TextureCreateSolid`, a null required image `CStatus`, and an atlas flag placed
in the BLP version field. The latter two release runs passed 31/32 checks.
Fixtures now use qualified BLP Services, valid status/readiness checks and the
correct flag byte. Superseded sanitizer runs were cancelled and are not qualified;
no detector or decoder policy was weakened. Local source/launcher checks: 50
passed, one CMake availability skip; thirteen original Windows function bodies
and whitespace checks passed.
