# Original Gxu native integration

This work follows merged PR #26 (`c3ef9fd989d15beb4af2695ea0cc08865e23bd19`)
and PR #27 (`6dae38e729a44c8df2424a0f27d6b892fcac7b0f`). It selects the
product's original Gxu face, font, layout/string, glyph/kerning/row caches and
batched consumers plus original Services HTEXTFONT/HTEXTBLOCK wrappers into a
separate native ABI target. It does not establish
GlueXML/FrameXML login, networking, authentication or gameplay. M3 stays open.

`WowNativeGxu` explicitly selects the original sources and scalar Base/Tempest
dependencies. `LegacyFonts` requires the installed native device and its current
main-thread context. Destroy strings/batches and close external HFACE references
before the font owner, then destroy the device/context. Keep a Services font
handle alive until its text blocks are closed, matching the original Frame
caller contract. Native public Gxu/Services font entry points, direct font/string
cache mutation and handle refcount changes reject foreign threads before mutation.

The native face provider returns completed font reads, bounded to 32 MiB. In an
application those bytes come from authenticated original-key packages on the
retained file worker, using existing registered-MD5 policy. Each actual FACEDATA
owns its bytes until FT_Done_Face, and the original hash/self-reference/duplicate
handle counts control eviction. Failed face parsing publishes no cache entry.
Native requests are never cast to legacy SFile or CAsyncObject handles.

The callback texture bridge creates actual CTexture/CHandleObject instances and
CGxTex resources for the original 256x256 ARGB4444 atlas. It supports this bounded
callback contract; disk/async image loading and the full Services texture cache
remain separate work. Original glyph nibble packing, row placement, layout,
kerning and PCT batch generation remain in their original consumers. Native
raster ownership validates atlas bounds and copies signed-pitch rows before
FreeType reuses its slot. The scratch atlas clears between pages. Original
Windows bodies and their historical provenance are retained; native adaptations
make no new OFFSET/VERIFY claim or donor attribution.

The concrete adapter now binds the initial drawable before original consumers
submit and reports original OpenGL `ISetCaps` pixel/texel-center semantics.
The three required scalar pixel conversions retain the original float epsilon
and rounding expressions, with bounds checks before native integer conversion.
They do not select the broader, unaudited Tempest soft-float implementation.

`LegacyGxuCheck` runs the actual public Gxu API and original consumer methods.
Synthetic CI uses authored geometric fonts/shaders, with a separate Unicode-alias
font for deterministic eight-page LRU pressure. It checks duplicate handles,
malformed faces, owner affinity, markup extents, line starts, glyph reuse,
red/white rendered pixels, stable repeated rendering, eviction/regeneration,
size invalidation, mono/gray/outline uploads and repeated consumer shutdown.
Original-data mode authenticates original UI shaders and Friz Quadrata through
the retained worker. Synthetic cache pressure and original-data output are
separate evidence. Font styling is not full UI/world/material coverage.

All client/dependency compilation runs in M3 Actions. Implementation
`ade47622c9023fee38fce1b6346be2f8be51fce4` is qualified in
[PR #28](https://github.com/RovxBot/wowdecomp/pull/28), merged as
`d07e289755d303accd0f97d7d49bcfe21b84d538`. The
[durable record](gxu-validation.json) retains nine passing implementation PR
workflows, 22 CTest checks per Linux GL configuration, three verified archives
and 33 packaged binary hashes/types. Full Windows x64 compilation used separate
matching-tree PR merge checkout `ca7e229e536c5434c3e9c6d26f6c8bf03494eb6a`,
with signature checks enabled. No Windows runtime or Win32 execution is implied.

Exact-source RX 7900 XTX / Mesa 26.2.5 / SDL X11 release and strict
ASan/UBSan/leak runs passed three original-data Gxu/Services cycles and three
authored eight-page cache-pressure cycles each, with empty stderr and zero
workers. The original font has no glyphs in the tested stress range: those
original-data cycles establish rendering, while authored aliases establish LRU
eviction/regeneration. Styled glyph uploads are covered; complete styles,
languages, shaping, gradients, shadows, stereo and world-space visual parity
remain unqualified. The red/white original glyph capture was inspected.
Prior legacy Gx, independent font/shader preview and four resource regressions
also passed. No new physical-input, macOS graphics or server acceptance is
claimed; M3 remains open and the interactive user sign-in preference is unchanged.

The first release link omitted three scalar conversions (`bb217f9`);
`a506069` then exposed the uninitialized initial drawable; `34013b5` exposed
unstable exact pixels after authored eviction with the missing original OpenGL
pixel-center capability. These failed jobs remain in the record. Superseded
matrix runs cancelled before all jobs finished are not clean passes. No detector
was disabled and image comparisons were retained.

After downloading and verifying the selected Actions artifact, run without
local compilation:

```sh
python3 Tools/Porting/run_native_gl.py \
  --artifact /tmp/wowdecomp-gxu-release --legacy-gxu \
  --data /mnt/games/3.3.5aClean/Data \
  --settings /tmp/wowdecomp-gxu-release-settings \
  --cache /tmp/wowdecomp-gxu-release-cache \
  --output /tmp/wowdecomp-gxu-release-runtime.json \
  --snapshot /tmp/wowdecomp-gxu-release.ppm --timeout 180
```

Use the sanitizer artifact with `--sanitizers` and separate outputs/user roots.
Omit data/settings/cache and pass `--font Tools/NativeGx/fonts/cache.ttf` for the
separate authored cache-pressure fixture. Each Gxu execution runs three cycles.
Its snapshot is automatic hidden-window readback, not physical-input acceptance.

## Next original login consumers

Source inspection at the parent main identifies these remaining dependencies;
this is a work order, not new compilation or runtime evidence:

| Caller | Required next integration |
| --- | --- |
| `Frame/CSimpleFontStringAttributes.cpp`, `CSimpleRender.cpp`, `Scrn/Stock.cpp` | Select the actual Frame consumers, preserve Services font-before-string lifetime and translate SDL size events to original Gxu invalidation. Keep the font owner's initialization separate from legacy `IStockInitialize` until that caller is adapted. |
| `Services/Texture.cpp`, Frame texture consumers | Adapt authenticated file-backed image ownership, supported mip/filter/format contracts and original async cancellation; the callback-only font atlas bridge does not provide these operations. |
| `FrameScript/FrameScript.cpp`, bundled `lua-5.1/src` | Adapt the product's pool allocator, modified Lua state/profile API, libraries, taint/secure environment and callbacks; a generic host Lua library is not a substitute. |
| `FrameXML/FrameXML.cpp`, `LoadXML.cpp`, bundled Expat | Adapt retained file ownership and original TOC/XML/Lua traversal. Preserve the separate interface signature: the original `.toc.sig` has 276 bytes, a signed digest and filename-bound RSA verification, followed by the computed content digest. MPQ signaturefile authentication alone does not qualify this interface signature. |
| `WoW/Client.cpp::InitializeClient`, `Glue/CGlueMgr.cpp` | Audit the broader initialization graph and script/event registrations, including client services, models, audio/movies and agreement requests. Original GlueXML loading and its frame digest checks precede actual login acceptance; do not satisfy required operations with success stubs. |

Continue with focused Lua/XML/Frame and image-service work packages, preserving
original data, addon APIs, authentication and signature defaults. Render the
actual original login UI before asking the user for interactive sign-in. Native
transport, realm endpoints, character/world rendering and observed Warden/world
compatibility remain distinct later checks.
