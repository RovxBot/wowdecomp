# File-backed native image Services

This implements the next bounded image consumer after [original XML](XML.md),
using original `CTexture`/`HTEXTURE`, handle reference counts and Gx callback
textures. `Services/Portable/Texture.cpp` adapts the 12340 `Services/Texture.cpp`
file/cache boundary; it imports no donor code. The Windows implementation and
its historical OFFSET/VERIFY annotations remain unchanged.

`Services::Native::TextureService` requires the installed device, retained
`FileQueue` and authenticated `Packages`. Its explicit reader overload exists for
embedders/fixtures; the application package overload keeps original-key RSA and
registered file MD5 policy. Reads and BLP decoding execute on the retained worker;
cache/handle/GPU changes occur during owner-thread completion. `Poll` and wait
operations pump the shared file queue, including other consumers' callbacks.
No ticket is stored in `CTexture::m_asyncObject` or presented as `CAsyncObject`.

The initial surface is deliberately bounded:

- BLP2 version 1 palette, raw BGRA and DXT1/3/5 sources use the existing bounded
  decoder. All supplied mip extents are validated; this service uploads mip zero
  as RGBA8, retaining pixels for legacy Lock/Latch/Unlock callbacks.
- Call `TextureCreate` with explicit sampler bit 0, optionally bit 3, and nearest
  or linear filtering. Wrap U/V participate in the cache key. Extensionless names
  gain `.BLP`; other extensions, root escape and oversized names fail visibly.
- Default mip policy, anisotropy, image atlases, legacy load flags (including
  Frame's `0x17`), TGA/alternate lookup, substitutions, blobs and image streaming
  remain unavailable. Frame callers need explicit adaptation before integration.
- At most 32 live distinct images and 32 undelivered requests, 32 MiB per source and
  2,097,152 base pixels per image. Device dimension limits still apply. Duplicate
  handles share a request; completed cached pixels are bounded to 256 MiB.
- Wait modes are 0 (query) and 1 (bounded completion wait). Read, decode, cancel
  and GPU failures retain diagnostic status and return no loaded image. They do
  not silently manufacture a success texture.

Native `CStatus` copies use a literal string format, so percent sequences in
filenames/decoder diagnostics cannot become variadic arguments; copying a status
to itself leaves its list unchanged. The original Windows path is preserved.

The cache holds no extra handle reference. Closing the final handle removes its
entry and cancels its retained ticket; a running read keeps its bytes/entry until
exactly one completion. Recreating the same name receives a separate entry and
cannot receive the retired completion. Close all file handles before service
destruction; the service drains its own cancelled callbacks before freeing state.
Keep the device/context and queue alive until then. Gxu callback-only font atlas
handles continue to work without installing file-image services.

`NativeImageCheck` exercises independent palette/raw/DXT FBO samples, path and
sampler cache identity, original handle references, pending dimensions, owner
affinity, failure diagnostics, queued cancellation and last-handle close during
an actual controlled worker read. It runs three complete device/service cycles
in both Linux GL configurations. Optional external-data mode uses authenticated
original UI shaders and the original loading-bar image through this service,
without constructing Frame objects or executing GlueXML.

Compilation and hosted synthetic qualification run exclusively in
[M3 GL Actions](../../.github/workflows/m3-gl.yml); Windows full-client compilation
remains the independent regression check. Implementation qualification is pending.
Original-data runtime execution, actual Frame factories/layout/input, broader
image contracts and original login rendering remain open. This does not establish
macOS graphics, Windows runtime, physical-input or server/gameplay acceptance.
