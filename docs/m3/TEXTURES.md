# Native Gx sampled textures and atlas updates

This M3 package extends the retained texture/render-target owner to ordinary
sampled textures, including the ARGB4444 format used by the original Gxu font
atlas. The synthetic and original UI-shader probe now uses this owner for its
RGBA images and DXT1 upload. Raw probe texture names and direct image redefinition
are removed. This is resource integration toward the original UI; original
font rasterization, GlueXML/login and the complete legacy device remain open.

## Source and caller audit

Source reference: product 12340 at merged main
`fd0ae32545f6da42b4fa58572eb26953131324c7`, following merged render-target,
transform and buffer PRs #20–#22. Their original qualification run/commit
identities remain intact; the merge identities are appended to those records.
No donor implementation, new dependency, original address or VERIFY claim is
imported.

| Source contract | Adaptation and limits |
| --- | --- |
| `CGxDeviceOpenGl::ITexDownload`, original format tables | RGBA/BGRA8888, ARGB4444/1555, RGB565 and matching DXT1/3/5 uploads with bounded CPU spans, explicit byte strides and level/face/rectangle checks. Padded rows are packed before GL mutation. Compressed offsets are block aligned; partial edge blocks and mip tails follow S3TC rules. Format conversion between uncompressed inputs/storage uses GL. Compressed-to-uncompressed fallback and UV/float/depth CPU formats fail visibly. |
| `TEXTURECACHE::CreateTexture/TextureCallback/UpdateDirty` | Original 256x256 font atlas uses ARGB4444 words and a 512-byte row stride. The new owner accepts that format and bounded subrectangles without casting native storage to `CGxTex`. The fixture uses synthetic atlas pixels; it does not invoke the original glyph rasterizer/cache, callback lock/latch/unlock, or render original font glyphs. |
| `ITexSetFlags`, `CGxTexFlags` | Preserve six filter mappings and clamp/repeat U/V. Explicit anisotropy requests validate the optional extension/device limit; unavailable requests fail. Rectangle textures require clamp and nonmip filters. Settings mutate the existing retained storage and preserve caller bindings. Automatic mip generation, force-mip tracking, base-mip rebasing and complete legacy dirty flags remain caller integration work. |
| `GlRenderTargets::Create/Bind/Sample` | Sampled textures and attachments share handles and feedback checks. Fixed dimensions/formats/mip counts, bounded 256 MiB total resource storage, and zero-filled ordinary sampled levels/faces. Renderable resources retain clear-before-read behavior. Compressed/sample-only resources cannot be attachments; CPU updates to current attachments fail. Samplers retain storage until unbound. |
| Original UI-shader probe | Uses owned sampled textures, buffered PCT triangles, qualified target/transform services and original unchanged `0x10003` UI programs. Original-key authentication, registered-MD5 policy and original assets remain enabled. A loading-bar image is not a login UI or gameplay observation. |

All operations and final handle release require the live owning context on the
process main thread. Native upload sources are retained by their caller through
the synchronous call; no native file ticket is substituted for `CAsyncObject` or
a legacy texture callback. A rejected source span/format/region changes no GPU
bytes. GPU errors remain visible. Uploads preserve texture/PBO bindings, unpack
alignment/row/image/skip/byte-order state and optional compressed-block unpack
state. They never interpret a CPU pointer as an offset into an inherited PBO.

The owner allocates all requested mip levels and cube faces. It does not resize
or redefine storage behind retained attachments/samplers. Replacing an image in
the probe allocates a new retained resource; the previous sampler keeps its old
allocation until replaced. Fixed allocation and transfer budgets are explicit
native bring-up limits, not claims about the full original texture cache.

The primary API contracts are the [S3TC specification](https://registry.khronos.org/OpenGL/extensions/EXT/EXT_texture_compression_s3tc.txt)
and [compressed unpack-state specification](https://registry.khronos.org/OpenGL/extensions/ARB/ARB_compressed_texture_pixel_storage.txt).
No additional asset format or optional effect is enabled.

## Validation

All compilation runs in [M3 Actions](../../.github/workflows/m3-gl.yml), through
`linux-gl` and `linux-gl-asan`. `GlTexturesCheck` exercises three context/resource
cycles: packed color channels, padded uploads and untouched texels, mip and cube
faces, DXT rows/NPOT edges/tails, zero initialization, filter/format/size/owner/
thread rejection, hostile unpack state and undersized inherited PBO preservation,
attachment-update rejection, synthetic ARGB4444 buffered blending and sampler
lifetime. The runtime wrapper verifies `--texture-checks` before any execution,
records it separately and fails the combined run on any fixture/probe failure.

After downloading a passing Actions artifact outside the repository:

```sh
python3 Tools/Porting/run_native_gl.py \
  --artifact /tmp/wowdecomp-m3-textures-release \
  --render-target-checks --transform-checks --buffer-checks --texture-checks \
  --data /mnt/games/3.3.5aClean/Data \
  --settings /tmp/wowdecomp-m3-textures-settings \
  --cache /tmp/wowdecomp-m3-textures-cache \
  --frames 30 --timeout 120 \
  --snapshot /tmp/wowdecomp-m3-textures.ppm \
  --output /tmp/wowdecomp-m3-textures-release.json
```

Use the sanitizer artifact with `--sanitizers --cycles 3` and separate user roots
and report. Compilation remains hosted only. Actual GPU execution and physical
input acceptance remain separate from hosted Mesa/Xvfb fixtures. Original fonts,
login UI, portable networking, character selection/world entry and Warden session
outcomes remain open. Keep the external server revision discrepancy in
[server identity](SERVER_IDENTITY.md).

Selected implementation `8e4fcde51d7962411d27c462655ad677f95aa23c` in
[PR #23](https://github.com/RovxBot/wowdecomp/pull/23) is qualified. The
[durable evidence](texture-validation.json) retains all eight passing PR workflows,
19 CTest passes in each Linux GL configuration and full Windows x64 compilation
at separate merge checkout `dc4bde6d308abb9d36d3b6d8cc29f5e2552319e5`.
ZIP digests, twelve packaged Linux binaries per configuration, notices and all
three Windows AMD64 binary identities matched their manifests. All 30 Python
checks ran on hosted Linux; local checks passed 29 with one CMake selection check
unavailable. File references, JSON/identities and whitespace passed.

Exact-source RX 7900 XTX / Mesa 26.2.5 / SDL X11 through Xwayland release and
ASan/UBSan/leak-enabled texture, buffer, transform and target checks each passed
three cycles. The original-data shader probe passed one release and three
sanitizer cycles with empty stderr and zero workers at shutdown. Original-key
signature authentication covered 28,948 records. Both original UI vertex variants
uploaded; permutation zero rendered. Both shader members lack registered MD5s
under the unchanged selective policy. The 960x640 loading-bar capture was
inspected. No sanitizer suppression, preload/audit shim, new physical-input
acceptance, original font/login rendering or native server session is claimed.
The user will sign in interactively when native authentication is available.
