# Native legacy Gx UI dispatch

Implementation starts from qualified Storm string handoff
`7f4ca29b95379505f07ae74f4834ff5f99ec0693` (PR #26, still open at this work's
start). This is a bounded concrete `CGxDevice` adapter for the native Linux
compatibility context. It retains the original global Gx entry points, render
state stack/cache, `CGxTex`, `CGxPool`, `CGxBuf` and `CGxShader` objects. It does
not complete original Gxu HFACE/layout/cache, GlueXML/login or world rendering.

The SDL context remains caller-owned. The device publishes itself only after
initializing capabilities, logical window geometry and stream pools; consumers and retained file reads
must finish before device destruction. Derived teardown releases sampler/target,
staging-write, buffer and program ownership before common CPU-object destruction
and before the context. One installed device belongs to the main/current-context
thread. Common state mutations reject foreign threads before changing the cache.
The original `GxDevDestroy` route retires consumers and deletes the adapter while
leaving the SDL/context owner with its caller. The native lifetime hooks do not alter the Windows vtable.

The first texture contract is single-mip 2D UI/cache images in matching RGBA/BGRA
8888, ARGB4444/1555 or RGB565 formats, nearest/linear filtering and anisotropy one.
Lock/Latch/Unlock callbacks supply a whole-image origin and row stride; dirty
rectangles are translated to native subregion uploads while borrowed bytes remain
locked. Render-target callbacks retain their notification sequence without uploading source
texels. A completed Lock receives exactly one Unlock even when Latch or validation
fails. Failures retain dirty status for retry. Callbacks must not mutate their texture. Reentrant immediate uploads and
destruction are rejected. A legacy callback supplies no allocation byte count, so the callback
retains responsibility for the declared full-image extent. Mips, compressed
callback sources/conversion, rectangle/cube targets and automatic mips are
explicitly unavailable in this first adapter, despite the underlying native
resource service having broader coverage.

The subsequent [authored mip-chain extension](IMAGE_MIPS.md) expands this original
qualified surface with explicit mip filters and increasing-level callbacks; its
qualification is recorded separately from the initial adapter evidence below.

Shader creation accepts an injected completed-byte provider instead of casting
native package/file tickets to legacy SFile handles. Application bytes must come
through authenticated original-key package reads on the retained worker. Bounded
`0x10003` decode and ARB upload finish before publishing cache entries; full member
path, requested permutation count and index form each cache key. A partially
released permutation set is repaired without duplicating surviving entries. Only arbvp1 and
arbfp1 UI coverage within the existing 94-register limit is qualified here.
Uploaded stereo permutations do not establish rendered stereo.

Legacy buffer lock pointers refer to retained native staging storage. Invalid
unlock extents leave it retryable. Destruction clears device and native bindings
before CPU deletion; pool cleanup also retires locked buffers. Draw submission
uses actual uploaded uint16 indices and native byte-coverage checks. Common
legacy matrices are authoritative and are synchronized through the qualified
native transform service before the original UI transpose API or submission.
Nonidentity texture matrices and texgen remain visibly unavailable. Native
state-cache comparison uses bit comparison instead of signed subtraction.

The PR #84 original model integration additionally admits `BufStream(target, 0, 0)`
as the original stream identity query used by model/particle teardown. It validates
the target and owner, returns the existing stream and preserves its allocation.
Partial-zero extents remain invalid. The existing integrated startup fixture
checks identity and item count; this adds no model rendering or larger device
capability claim. See [the continuation](integrated-model-render-validation.json).

## Validation

All client/dependency compilation runs in Actions. `LegacyGxCheck` is added to
the Linux GL allowlist, CTest and artifact manifest. Its authored fixtures call
actual global Gx dispatch through the concrete adapter: cache lookup, failed
stride/Latch uploads and retry, dirty-region pixels, PCT stream lock/unlock,
indexed FBO draws, actual-index bounds, unsupported texgen, pushed-state resource
retirement, foreign-thread state rejection, partial shader-cache reuse, legacy
readiness and locked-pool/consumer teardown. It repeats context/device shutdown three
times. External-data mode reads the original two UI vertex permutations, pixel
shader and Friz Quadrata face through authenticated packages and the retained
worker, then checks an original glyph rendered through legacy texture callbacks
and PCT batches. This uses the qualified native rasterizer, not original Gxu
layout or the original login UI.

Qualified implementation: `6ef31f4a94cef0d1b1947f46e4e35d716568b520` in
[PR #27](https://github.com/RovxBot/wowdecomp/pull/27), stacked on PR #26. The
[durable record](legacy-device-validation.json) retains all nine passing
implementation PR workflows, 21 CTest checks per Linux GL configuration, three
verified archive digests and all 31 packaged binary hashes/types. Full Windows
x64 compilation used separate PR merge checkout `d319ec4bef5ed042ce8b45fb62923b5f927ecd60`;
its tree matches the selected implementation, with signature checks enabled.

Exact-source RX 7900 XTX / Mesa 26.2.5 / SDL X11 release and strict
ASan/UBSan/leak checks each passed three original-data legacy Gx cycles, including
actual global dispatch, source callback failure/retry, dirty-region FBO pixels,
original Friz Quadrata glyphs through original UI shaders and repeated consumer/
resource/context teardown. Each execution had empty stderr and zero probe workers.
The independent original-data preview passed one release and three sanitizer
cycles; target, transform, buffer and texture fixtures each passed three cycles
in both modes. Its inspected 960x640 capture remains separate from the legacy
fixture's FBO readback and from original Gxu/login or physical-input acceptance.
Both UI shader members and the original font lack registered MD5 entries;
original-key signature authentication (28,948 records) and selective MD5 policy
remain enabled.

The initial scalar-link omission, below-minimum authored texture fixture and
missing abstract draw-list RTTI link failures remain recorded. Native empty
interface destructor definitions retain UBSan vptr instrumentation; no detector
was disabled. Local source checks passed (34 passed, one unavailable CMake check
skipped); no client or dependency compiled locally. M3 remains incomplete, with
original Gxu handle/layout/cache and texture-service consumers, GlueXML/login,
transport/authentication, interactive sign-in and observed GrimGuzzler world entry
still open. No server request or policy change was made.

For downloaded, hash-verified Linux artifacts, run three original-data legacy
fixture cycles without compiling locally:

```sh
python3 Tools/Porting/run_native_gl.py \
  --artifact /tmp/wowdecomp-m3-legacy-gx/SELECTED_ARTIFACT \
  --legacy-device --data /mnt/games/3.3.5aClean/Data \
  --settings /tmp/wowdecomp-m3-legacy-gx-settings \
  --cache /tmp/wowdecomp-m3-legacy-gx-cache \
  --output /tmp/wowdecomp-m3-legacy-gx-runtime.json --timeout 180
```

Use the sanitizer artifact with `--sanitizers` and separate report/user roots.
`--legacy-device` selects `LegacyGxCheck` and its own manifest hash, not the
independent `NativeGlCheck` preview. It always runs three cycles and currently
uses enUS without a snapshot. The old preview and four resource checks remain
separate regressions. Pixel readback isolates pack/PBO state, and scene clear
temporarily enables color/depth writes before restoring their previous masks.
No new physical-input, macOS graphics, Windows gameplay or server acceptance is
implied by these fixtures.
