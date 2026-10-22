# Original Frame texture regions

This package selects original 12340 `CSimpleTexture` consumers for the native
Frame hierarchy. Texture bodies move from `CSimpleRender.cpp` to the explicitly
selected `CSimpleTexture.cpp`; Windows-selected bodies and extracted XML helpers
retain their original tokens. Historical address/VERIFY tags are provenance,
not new binary verification. All client/dependency compilation runs in Actions.

Textures use the original class allocator with native alignment, rollback and
mixed heap/pool deletion. Parent/root ownership and deferred region teardown are
shared with FontStrings. The Scripts owner installs Texture metatables and UI /
Desaturate pixel shaders; all script objects close before that owner, then image
Services, renderer/Fonts/Gxu/layout/XML/VM/device/context/queue. Retained batch
quads duplicate image handles and shader references, so region destruction cannot
free resources still used by a batch. Texture coordinates invalidate retained
batches after mutation. Native fields initialize before original flag masks.

Lua CreateTexture, XML Layers/Texture, exact-key inherited templates, names and
parentKey use existing canonical-table and exception-safe registry contracts.
Selected methods expose images, visibility, colors/gradients, alpha, coordinates,
rotation, blend modes, draw layers and desaturation. Numeric admission rejects
nonfinite coordinates. Original animation services and Lua tiling mutation remain
unavailable; XML enabled tiling is rejected until sampler reconfiguration is
qualified. General animation/widget/model/backdrop/scroll consumers remain open.

The native solid-image adaptation owns original 8x8 ARGB callback pixels, with
GPU teardown before pixel storage and the original opacity threshold. It creates
independent handles rather than exporting the original global solid-image cache.
File images retain the existing BLP2, sampler, local wait 0/2, atlas and async
service contracts and limits. Blocking collection propagates read/decode/upload
failure instead of leaving a failed image as an endless delayed load. Native
image tickets remain separate from legacy CAsyncObject.

The hierarchy fixture exercises solid/file replacement, shader desaturation,
coordinate invalidation and exact repeats, alpha/hide/rebuild, canonical Lua and
XML names/parent keys, stale-table rejection and pooled teardown each owner cycle.
Original-data mode loads the unchanged loading-bar image and original shaders from
the read-only clean installation; composition is authored. Existing original-key
MPQ authentication, registered member MD5 checks and GlueFonts-subset InterfaceKey
checks remain enabled. Complete ordered signed GlueXML execution and the full
second digest gate remain separate requirements.

Hosted/runtime qualification is pending for this implementation. This does not
establish original login rendering, physical IME/edit-box acceptance, transport,
authentication or gameplay. Remaining widget factories and nonempty Lua handlers
are the next UI callers. The user will sign in interactively when native login is
ready; settings/cache stay outside game data. Preserve server revision discrepancies
and all historical evidence. M3 remains open.
