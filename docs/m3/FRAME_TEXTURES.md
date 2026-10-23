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
XML names/parent keys, retained templates, stale-table rejection, authored atlas
sampling and growth beyond the original 736-slot pool block each owner cycle.
Original-data mode loads the unchanged loading-bar image and original shaders from
the read-only clean installation; composition is authored. Existing original-key
MPQ authentication, registered member MD5 checks and GlueFonts-subset InterfaceKey
checks remain enabled. Complete ordered signed GlueXML execution and the full
second digest gate remain separate requirements.

The qualification below records hosted and downloaded runtime execution. This does not
establish original login rendering, physical IME/edit-box acceptance, transport,
authentication or gameplay. Remaining widget factories and nonempty Lua handlers
are the next UI callers. The user will sign in interactively when native login is
ready; settings/cache stay outside game data. Preserve server revision discrepancies
and all historical evidence. M3 remains open.

## Qualification

The [original Frame texture package](FRAME_TEXTURES.md) in
[PR #72](https://github.com/RovxBot/wowdecomp/pull/72) is qualified at
`6727ee12de9dd44a74dd548b63764efe35da0244`, merged as `519be5f497074a582419f0a9276e73d82ff69c3a`.
[Durable validation](frame-texture-validation.json) records thirteen passing
final-head PR workflows, 38 CTest checks per Linux release/strict ASan/UBSan/leak
configuration and full signature-enabled Windows x64 compilation at matching-tree
checkout `a8c63b738b0b45081f7c71bb77b9e3182b6d9986`. Three archives/all 51 packaged binary
hashes/types/notices verified. Implementation, hosted checkout and merge share one
source tree. After the interruption, fresh RX 7900 XTX / Mesa 26.2.5 / SDL X11
release/strict packages passed 30 three-cycle authored/original-data desktop
executions on 2026-10-24, with empty stderr and zero workers. Four resource fixtures
passed three cycles per mode; all fifteen final capture pairs match the prior baseline.

Original CSimpleTexture regions now use native-aligned original pools, canonical
Lua CreateTexture and exact-key retained XML templates/layers. Selected image,
visibility/color/gradient/alpha/coordinate/rotation/blend/draw-layer/desaturation
methods use existing Services/Gx and retained quads. Preserve native flag
initialization, raw image-mode/sampler admission, dirty coordinate/nonblocking
updates, retained image/shader handles and GPU-before-solid-pixel teardown.
Original 8x8 ARGB solid extents/opacity are preserved without the global solid cache.
The fixture covers growth to 737 slots, authored atlas coordinates, original
UI/Desaturate shaders and original loading-bar pixels, plus XML/name/parentKey/
stale-table and repeat/replacement/visibility/alpha lifetimes.

Close objects before Scripts/metatables/image-mode shaders, then image Services,
renderer/Fonts/Gxu/layout/XML/VM/device/context/queue. Animation and tiling mutation
remain unavailable. Windows-selected texture/Frame/renderer bodies and extracted
XML helpers retain original tokens. The missing-declaration native compiler failure
and Windows orientation-table extraction failure remain recorded; no detector or
pixel assertion was disabled. All compilation ran in Actions. Original-key archives,
selective MD5 and before/after GlueFonts-subset InterfaceKey checks remain enabled.

Composition is authored, separate from full login. Intermediate GPU readbacks assert
texture pixels; final captures remain the historical authored hierarchy text.
Remaining button/edit-box/backdrop/animation/model/scroll widgets, nonempty Lua
handlers, complete signed ordered GlueXML/full second digest gate, actual login,
transport/authentication and observed gameplay remain open. Physical IME/edit-box/
candidate acceptance remains pending. No new Windows runtime, macOS graphics or
server acceptance; preserve server revision discrepancies, interactive sign-in and
read-only clean data. M3 remains incomplete.
