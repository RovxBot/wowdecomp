# Legacy Gx integration audit

This is a source audit and implementation boundary for the work after the native
font preview. It does not report a completed native device, original login UI or
gameplay. Source reference: combined font/packet tree
`da1451336d49be27e391be09b7b829ff7e4306ca`.

The native `GlContext`, program, target/texture, transform and buffer services
exercise the original UI shader path. At the audit reference above, the legacy
`Gx.cpp` entry points still dispatched through `g_theGxDevicePtr` outside the native
allowlist. The subsequent [bounded legacy device adapter](LEGACY_DEVICE.md) in
[PR #27](https://github.com/RovxBot/wowdecomp/pull/27) now selects those entry points
and the common device/resource objects, with a concrete native UI adapter.
Its qualification is recorded separately; the full device, original Gxu caches
and login integration requirements below remain broader than that UI subset.

| Existing source/callers | Required adaptation |
| --- | --- |
| `Gx/CGxDevice/CGxDevice.cpp`, `Gx.cpp` | Retain the common device's state stacks, dirty tracking, resource lists and shader dispatch. Audit constructor `IRsInit`/processor/capability dependencies before publishing the global device. Select native platform sources explicitly; the Win64 implementation also selects D3D and Windows APIs. |
| `CGxTexFlags.cpp`, `CGxTex.cpp`, `CGxDeviceOpenGL::ITexMarkAsUpdated/ITexDownload` | The flags constructor dereferences the global device for anisotropy limits; constructing a texture also default-constructs its flags before assignment. Preserve initialization order or provide an explicit native capability boundary. Adapt real `CGxTex` objects and Lock/Latch/Unlock callbacks, mip/face/dirty rectangles and source-format rules. Keep callbacks' source bytes alive through their upload and pair successful locks with one unlock on failures. Do not store a GLuint by aliasing a pointer field. |
| `CGxPool`, `CGxBuf`, `Buffer.cpp`, device stream/vertex/index methods | Adapt actual pool/buffer objects and stream/lock contracts to retained native buffers. Preserve uploaded coverage and indexed bounds; retain staged allocations until commit/abort. Clear legacy bindings before deleting their CPU objects, and release native storage while its context is current. |
| `CGxShader::Load`, device shader hash/permutation/cache methods | Connect legacy shader objects to the bounded original `0x10003` decoder and native program ownership. Audit the legacy SFile calls and borrowed-code lifetime. The current UI service's 94-register range and supported ARB dialect are explicit limits; world shader/material coverage needs original assets and actual rendering checks. |
| `CGxDevice` transforms and `IRsSendToHw` | Route the common matrix/state cache to the native transform/raster owner. Avoid two independently mutable copies of device state or bindings. Preserve target projection/culling and normalized viewport/scissor rules. Unsupported nonidentity texture matrices and other unavailable states must fail visibly until implemented. |
| `GxuFontFace`, `GxuFontFont`, `GxuFontTextureCache`, `GxuFontMiscClasses` | Reuse the bundled native FreeType module fixes, then adapt original handles, file ownership, glyph/string caches and batched rendering. The original font atlas callback supplies a 256x256 ARGB4444 image; its font flags select nearest/linear with anisotropy one. The current memory-face/text preview does not export these legacy objects. |

## File, window and shutdown dependencies

`CGxShader` and original Gxu faces consume legacy `SFileImpl`/SFile entry points.
The native authenticated package/file services have a separate ABI. Adapt their
callers and ownership instead of casting native handles or file tickets to
`SFileImpl`/`CAsyncObject`. Preserve original-key authentication, registered MD5
checks, archive precedence and owner-thread callback delivery. File reads remain
on the retained worker; GPU/cache changes belong to the main/context thread.

SDL remains the sole native window/context owner. A native device must translate
its original window/input entry points to that owner and keep logical/pixel sizes
separate. Windows HWND/HDC/HGLRC and WGL procedure ownership stay in Windows
implementations. Existing calling-convention declarations, including
`GxDeviceCallback`, also need an explicit native ABI instead of defining WIN32 or
assuming that 64-bit pointers imply the Windows ABI.

Shutdown must first stop consumers and drain retained file callbacks, then release
legacy caches/resources and native sampler/attachment/buffer/program references,
then destroy the current context and SDL window. The common device destructor
has its own resource-list cleanup; a concrete adapter must audit its interaction
with derived destruction and virtual dispatch before relying on it for GPU frees.

## Qualification required for this boundary

The next runnable check must call actual legacy Gx/resource callbacks and batches
through a concrete native device, with original shader/font output, checked FBO
pixels, update/failure lifetimes and repeated strict sanitizer teardown. A header
or object-file compile alone is insufficient. The native window must remain
usable under the existing resize/focus/fullscreen/close contract; operator
observations remain separate from injected fixtures and automatic readback.

After that dispatch works, bring up original Gxu/Lua/GlueXML/FrameXML and interface
signature verification to render the actual login UI. Preserve the Windows
OpenGL comparison failure and missing fixed-scene captures rather than claiming
world/material parity from UI pixels. [Packet prerequisites](PACKETS.md) are
qualified separately; socket/DNS/worker dispatch, authentication and the observed
GrimGuzzler session still remain. The user will sign in interactively, and the
[external server revision discrepancy](SERVER_IDENTITY.md) remains unchanged.

The next [legacy string prerequisites](LEGACY_STRINGS.md) select the original
Storm hash/name/ordered-format code for the native ABI and exercise actual
hash-cache allocation/lifetimes. It is qualified at `2d946465e52d7ccfb43722d844d76a4fa4cd038b`
in PR #26; [durable evidence](string-validation.json) records hosted, binary and
original-data regression checks. This does not change the device/async/login
qualification boundary above.
