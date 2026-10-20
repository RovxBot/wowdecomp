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

The initial qualified surface below describes PR #35. The subsequent
[authored mip/readiness extension](IMAGE_MIPS.md) adds explicit mip filters,
local wait mode 2 and UI priority; its qualification is recorded separately.

The initial surface is deliberately bounded:

- BLP2 version 1 palette, raw BGRA and DXT1/3/5 sources use the existing bounded
  decoder. Original Gx requires power-of-two dimensions of at least 8x8.
  All supplied mip extents are validated; this service uploads mip zero
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
remains the independent regression check.

## Qualification

[PR #35](https://github.com/RovxBot/wowdecomp/pull/35) is qualified at
`db24f90dc2d845d5a5944df0cedb46fd116d13ec`, merged as
`b1479843dd7eac430514fc1103a4e9c1e6e39301`. The [durable record](image-validation.json)
retains all twelve passing implementation PR workflows, 23 CTest checks per
Linux GL configuration, three verified archive digests and all 35 packaged binary
hashes/types. Full Windows x64 compilation with signatures enabled used separate
matching-tree checkout `b3df743c8c009d1076d9ec4acfbe3cbd6ba50e86`.

Downloaded release and strict ASan/UBSan/leak binaries each passed three authored
and three original-data image cycles on RX 7900 XTX / Mesa 26.2.5 / SDL X11 on
Xwayland. Original-key signaturefile authentication (28,948 records) and selective
MD5 policy remained enabled. The original 64x64 loading-bar image rendered through
actual Services handles/PCT/original UI shaders into a 32x32 FBO. Release and
sanitizer capture hashes match; the release capture was inspected. Captures remain
external. This does not execute FrameXML or render the login screen.

Original-data Gxu and legacy Gx regressions passed three cycles per configuration;
the preview passed one release/three sanitizer cycles. Texture, buffer, target and
transform fixtures each passed three cycles in both modes. All eighteen runtime
executions exited 0 with empty stderr and zero probe workers. Local source/launcher
checks passed (43 passed, one platform skip); no client/dependency compiled locally.

The first graphics fixture failed because authored sources were smaller than
original Gx's 8x8 minimum. The next release fixture used the opposite of the
original offscreen Y projection and RGB arguments for the original BGR constructor;
its sanitizer job was cancelled by the next revision. These failures remain in
the record. Authored inputs/expectations were corrected against the original source;
the renderer restrictions and all sanitizer detectors remain intact.

## Next original callers

| Caller | Required integration |
| --- | --- |
| `CSimpleTexture::SetTexture` in `Frame/CSimpleRender.cpp` | Adapt its `0x17` path/priority/atlas flags and actual image-mode shaders; preserve handle replacement and error reporting. Do not silently discard unsupported flags to use this first service. |
| `CSimpleTexture::Draw`, dimensions/embedded texture consumers | The qualified [mip/readiness extension](IMAGE_MIPS.md) now supplies explicit authored mip filters and local wait 2. Adapt actual image coordinates and remaining atlas/default-policy contracts while preserving original offscreen orientation. |
| `CSimpleFont`, `CSimpleFontString`, layout/root owners | Connect actual Frame font/string factories to Gxu/Services, keep font handles alive through blocks/batches and invalidate fonts/layout on SDL size changes. |
| `FrameXML_ProcessFile`/`FrameXML_CreateFrames`, `CGlueMgr::Resume` | Connect actual factories, retained templates and ordered FrameScript execution; preserve the second digest comparison before FRAMES_LOADED. Then execute/render original GlueXML/login before interactive sign-in. |

For downloaded packages, verify the archive and `NativeImageCheck` manifest hash
before execution; all compilation stays in Actions:

```sh
SDL_VIDEODRIVER=x11 EGL_PLATFORM=x11 NativeImageCheck \
  --data /mnt/games/3.3.5aClean/Data \
  --settings /tmp/wowdecomp-image-settings --cache /tmp/wowdecomp-image-cache
```

Use the sanitizer package with `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`, with separate writable roots.
Actual Frame factories/layout/input, broader image contracts, original login
rendering, transport and observed gameplay remain open. No new macOS graphics,
Windows runtime, physical-input or server acceptance is recorded. Preserve the
server revision discrepancy and interactive user sign-in preference. M3 is open.
