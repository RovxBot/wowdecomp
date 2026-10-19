# Native legacy Gx UI dispatch

Implementation starts from qualified Storm string handoff
`7f4ca29b95379505f07ae74f4834ff5f99ec0693` (PR #26, still open at this work's
start). This is a bounded concrete `CGxDevice` adapter for the native Linux
compatibility context. It retains the original global Gx entry points, render
state stack/cache, `CGxTex`, `CGxPool`, `CGxBuf` and `CGxShader` objects. It does
not complete original Gxu HFACE/layout/cache, GlueXML/login or world rendering.

The SDL context remains caller-owned. The device publishes itself only after
initializing capabilities and stream pools; consumers and retained file reads
must finish before device destruction. Derived teardown releases sampler/target,
staging-write, buffer and program ownership before common CPU-object destruction
and before the context. One installed device belongs to the main/current-context
thread. The native lifetime hooks do not alter the Windows vtable.

The first texture contract is single-mip 2D UI/cache images in matching RGBA/BGRA
8888, ARGB4444/1555 or RGB565 formats, nearest/linear filtering and anisotropy one.
Lock/Latch/Unlock callbacks supply a whole-image origin and row stride; dirty
rectangles are translated to native subregion uploads while borrowed bytes remain
locked. A completed Lock receives exactly one Unlock even when Latch or validation
fails. Failures retain dirty status for retry. Reentrant update/destruction is
rejected. A legacy callback supplies no allocation byte count, so the callback
retains responsibility for the declared full-image extent. Mips, compressed
callback sources/conversion, rectangle/cube targets and automatic mips are
explicitly unavailable in this first adapter, despite the underlying native
resource service having broader coverage.

Shader creation accepts an injected completed-byte provider instead of casting
native package/file tickets to legacy SFile handles. Application bytes must come
through authenticated original-key package reads on the retained worker. Bounded
`0x10003` decode and ARB upload finish before publishing cache entries; full member
path, requested permutation count and index form each cache key. Only arbvp1 and
arbfp1 UI coverage and the existing 94-register limit are qualified candidates.
Uploaded stereo permutations do not establish rendered stereo.

Legacy buffer lock pointers refer to retained native staging storage. Invalid
unlock extents leave it retryable. Destruction clears device and native bindings
before CPU deletion; pool cleanup also retires locked buffers. Draw submission
uses actual uploaded uint16 indices and native byte-coverage checks. Common
legacy matrices are authoritative and are synchronized through the qualified
native transform service before the original UI transpose API or submission.
Nonidentity texture matrices and texgen remain visibly unavailable. Native
state-cache comparison uses bit comparison instead of signed subtraction.

## Validation

All client/dependency compilation runs in Actions. `LegacyGxCheck` is added to
the Linux GL allowlist, CTest and artifact manifest. Its authored fixtures call
actual global Gx dispatch through the concrete adapter: cache lookup, failed
stride/Latch uploads and retry, dirty-region pixels, PCT stream lock/unlock,
indexed FBO draws, actual-index bounds, unsupported texgen, pushed-state resource
retirement and locked-pool teardown. It repeats context/device shutdown three
times. External-data mode reads the original two UI vertex permutations, pixel
shader and Friz Quadrata face through authenticated packages and the retained
worker, then checks an original glyph rendered through legacy texture callbacks
and PCT batches. This uses the qualified native rasterizer, not original Gxu
layout or the original login UI.

Compilation and runtime qualification are pending. Local source-audit Python
checks pass (33 tests, one unavailable CMake check skipped); no local client or
dependency compilation was performed. Historical qualification records remain
unchanged. M3 remains incomplete, with transport/authentication, interactive
sign-in, character selection and an observed GrimGuzzler world session still open.
