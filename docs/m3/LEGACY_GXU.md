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

All client/dependency compilation runs in M3 Actions. Qualification is pending
the first hosted build, strict sanitizer fixtures, exact-source GPU execution
and Windows regression. No macOS graphics, physical-input acceptance, server
session or milestone completion is claimed. The existing external server
revision discrepancy and interactive user sign-in preference are unchanged.
