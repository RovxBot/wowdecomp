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
UBSan vptr checks remain enabled. Implementation qualification is pending.

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
