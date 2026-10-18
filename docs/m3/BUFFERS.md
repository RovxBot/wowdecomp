# Native Gx buffers and vertex submission

This M3 work package connects retained vertex/index storage to the existing
native context, targets, transforms and original UI shader probe. It replaces
the probe's immediate-mode quad with a streamed 24-byte PCT buffer and six uint16
triangle indices. The full native client remains gated. M3 still requires the
legacy device/cache/texture/async adaptation, original fonts/GlueXML/login,
portable networking and an observed unchanged-data GrimGuzzler world session.

## Source and caller audit

The source reference is product 12340 at
`2a92c398ef818e690c897ec5806ff4097a9fac3a`, including the qualified but unmerged
transform work in PR #21 and render-target work in PR #20. This work is stacked
on PR #21. No donor body, original binary address or VERIFY claim is imported.

| Original contract | Native adaptation and remaining work |
| --- | --- |
| `CGxPool`, `CGxBuf`, `BufCreate`, `BufData`, `BufStream` | Retained opaque allocations with validated target/usage, checked uint32 byte size and immutable element metadata. Static/dynamic/stream map to the original GL usage hints. Native allocations are individual buffers; legacy pool suballocation, invalidation, broker reuse and stream resizing remain caller-adapter work. |
| `CGxDeviceOpenGl::BufLock/BufUnlock` | A move-only write retains the allocation and owns CPU staging until commit/abort. Zero commit size means the whole allocation; explicit size publishes a prefix. Abandonment leaves uploaded bytes intact. Double lock, upload or draw during a write fail. Pointers expire when committed/aborted. This is an explicit native contract, not an ABI replacement for legacy lock pointers or emergency-memory/device-loss behavior. |
| `PrimVertexFormat`, `Buffer::s_vertexBufDesc`, `s_glAttribSize/Type/Normalized` | Validate raw indices, duplicates, byte extents and strides before replacing the format. Preserve all fourteen Gx ARB attribute indices and seven original type codes, including normalized versus unnormalized byte weights/indices, short pairs and float scalar/pairs/triples. Separate streams and zero tightly packed strides work. Packed color byte order is preserved; no implicit BGRA conversion is added. The PCT probe uses the original offsets 0/12/16 and stride 24. |
| `IStateSyncVertexPtrs`, `IStateSyncIndexPtr`, `CGxDeviceOpenGl::Draw` | Use a private VAO with ARB attributes and preserve caller VAO/array/element bindings. Original batch start/count count vertices or uint16 indices; actual indices and each fetched byte extent are checked against uploaded coverage. No base vertex or unchecked min/max hint is introduced. All six original primitive mappings are available. Enabled ARB vertex programs are required; fixed-function client arrays and texture-coordinate remapping remain unavailable. |
| Gxu UI/cursor streams and state ownership | The shader probe now submits buffered PCT triangles, with original shader constants and target orientation supplied by the existing transform owner. This does not export a legacy `CGxBuf*`, wire global Gx dispatch, render font glyphs or provide the complete dirty-state cache. Missing generic attributes use caller constants; callers must supply every shader input. |

All GPU access, handles' final release and staging-token destruction require the
owning live current context on the process main thread. Formats and index binding
retain allocations after caller handles are released. Buffers from another owner
are rejected, including owners sharing a context. Upload coverage and a CPU shadow
allow validation of sparse writes and actual index contents without GPU readback.
All validation/CPU allocation precedes upload. Buffer creation and upload use the
array target so they never replace an element binding in the caller's VAO.

This deliberately prioritizes correctness: retained CPU shadows, staging copies
and per-fetch bounds checks need profiling before world-scale rendering. No
performance improvement or full CGxDevice integration is claimed. Private VAO
storage expires before the context. Undefined GPU allocation bytes cannot be
drawn until the caller publishes corresponding bytes.

## Validation

All compilation runs in [M3 Actions](../../.github/workflows/m3-gl.yml), using
`linux-gl` and `linux-gl-asan`. The graph audit allowlists `GlBuffers.cpp` and
`GlBuffersCheck`, retaining native/legacy ABI separation and signature defaults.
The existing full Windows x64 reference remains a separate regression check.

`GlBuffersCheck` runs three complete context/resource cycles. It verifies buffered
PCT pixels for arrays and indexed draws with nonzero starts, original type-code
normalization and byte order, separate streams, zero strides, sparse uploads,
actual out-of-bounds indices, retained bindings, abandoned/partial/full staging
writes, invalid formats/specifications/overflows, foreign owners/threads, required
ARB state and caller VAO/array/element binding restoration. The existing synthetic
and original-data shader probe also uses buffered triangles for its texture,
depth/blend, transform and DXT1 checks. Hosted Mesa/Xvfb output remains separate
from actual GPU/desktop execution and operator input observations.

The hash-checking [runtime wrapper](../../Tools/Porting/run_native_gl.py) accepts
`--buffer-checks`, records the fixture separately, verifies every requested binary
before any execution and fails the combined run on a fixture/probe failure or
timeout. After downloading a passing artifact outside the repository:

```sh
python3 Tools/Porting/run_native_gl.py \
  --artifact /tmp/wowdecomp-m3-buffers-release \
  --render-target-checks --transform-checks --buffer-checks \
  --data /mnt/games/3.3.5aClean/Data \
  --settings /tmp/wowdecomp-m3-buffers-settings \
  --cache /tmp/wowdecomp-m3-buffers-cache \
  --frames 30 --timeout 120 \
  --snapshot /tmp/wowdecomp-m3-buffers.ppm \
  --output /tmp/wowdecomp-m3-buffers-release.json
```

Use the sanitizer artifact with `--sanitizers --cycles 3` and separate output/user
roots. Local compilation is prohibited. Original-key RSA and registered MD5
policy stay enabled. Historical physical-input acceptance keeps its own source
identity. This work does not qualify macOS graphics, new Windows gameplay,
original fonts/login or a native server session. The external server revision
versus package-tag discrepancy remains in [server identity](SERVER_IDENTITY.md).

Hosted and actual-GPU validation of this implementation is pending.

The initial hosted release at `5f5d2d9e255e468df9e0ee99b0f7e30b674952de`
compiled but failed buffer/probe pixels: the synthetic vertex programs read
conventional `vertex.color`/`vertex.texcoord[0]`, while buffered submission uses
Gx generic attributes 4/6. Those are separate inputs; the previous immediate-mode
probe supplied both. Synthetic programs now use the same explicit generic indices
as Gx submission. The [ARB vertex-program specification](https://registry.khronos.org/OpenGL/extensions/ARB/ARB_vertex_program.txt)
defines conventional and generic inputs separately. No original shader or native
buffer format is relabeled to conceal this fixture mismatch.
