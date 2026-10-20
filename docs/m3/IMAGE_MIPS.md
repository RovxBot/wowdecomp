# Authored image mip chains and local readiness

This extends the qualified [file-image Services](IMAGES.md) and [legacy Gx
adapter](LEGACY_DEVICE.md). It adapts this product's 12340
`CGxDeviceOpenGl::ITexMarkAsUpdated` lock/latch/unlock traversal and
`Services/Texture.cpp` priority/readiness boundary. No donor body, dependency,
game data or new OFFSET/VERIFY claim is imported. The original Windows paths
remain selected by the full Windows client.

Native file-image handles now accept nearest-mip-nearest, linear-mip-nearest and
linear-mip-linear filtering in addition to nearest/linear. Mipped requests require
the complete authored BLP2 chain through 1x1, with both dimensions clamped to one
at each level. Missing levels fail with diagnostic status; no levels are generated.
Nonmip requests validate the supplied chain and retain only level zero. Each sampler
variant keeps its own cache identity. Opacity considers every retained level.

The native Gx adapter allocates the required immutable mip storage and calls Lock
once at the original extent, Latch in increasing level order at each level's extent,
then Unlock once at the original extent. A completed Lock receives exactly one
Unlock even when a lower Latch/upload fails. Failed updates stay dirty for a full
retry. Mipped dirty updates refresh the entire chain, matching the original OpenGL
consumer; single-level updates retain their qualified dirty-rectangle behavior.
Changing a sampler across mip/nonmip allocation classes fails visibly and requires
resource recreation. Mipped render targets, anisotropy, generated mips, base-mip
aliasing, forced tracking, cube and compressed callback uploads remain unavailable.

Creation option bit `0x10` selects original UI priority 129 instead of ordinary
image priority 130. An identical cached request with this bit promotes a queued
ticket while preserving its handle/refcount. It cannot preempt a current read.
Wait modes are 0 (query), 1 (bounded completion wait) and 2 (wait for local data).
Original mode 2 avoids waiting when `CAsyncObject::m_char24` marks remote streaming;
the native service only supports local readers, so modes 1 and 2 both wait up to
30 seconds. Neither mode exposes native tickets as legacy async objects.

The existing 32-handle/request, 32 MiB source and 2,097,152 base-pixel limits remain.
A shared 256 MiB retained-pixel budget now includes all levels and retired requests
until their pixels are released. Worker-side temporary read/decode allocations are
separately bounded by the source and per-image limits. Owner-thread GPU/cache
mutation, callback delivery, cancellation and shutdown ordering remain required.

`NativeImageCheck` uses independent authored level colors to check magnification,
each minification level, trilinear blending, lower-level alpha, incomplete-chain
failure, wait 2, UI priority and duplicate-request promotion. `LegacyGxCheck` checks
nonsquare mip extents, failure at a lower level, exactly-once Unlock, retry,
whole-chain dirty updates and incompatible sampler/target rejection. Both retain
their three-cycle hosted release/strict sanitizer checks. Optional original-data
mode adds unchanged `Tileset/Elwynn/ElwynnDirtBase.blp` with trilinear minification
through actual Services/Gx/UI shaders, alongside the original loading-bar image.
Original-key package authentication and selective registered-MD5 policy stay active.

All compilation and hosted checks run in [M3 GL Actions](../../.github/workflows/m3-gl.yml).
Qualification and exact run/artifact/runtime identities are pending for this
implementation. The earlier [image validation](image-validation.json) remains
historical evidence for PR #35, not evidence for these new contracts.

Frame's `0x17` still requests disk lookup and atlas eligibility, which this native
surface rejects. Those flags must be adapted explicitly before selecting actual
Frame factories. Alternate TGA lookup, atlases, general async objects, actual
Frame/Lua/GlueXML/login rendering, transport and observed gameplay remain open.
This resource fixture does not qualify world rendering. M3 remains incomplete;
the external server revision discrepancy and interactive sign-in preference stay.
