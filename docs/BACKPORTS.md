# 4.3.4 backport investigation ledger

Product compatibility target: 3.3.5a (12340). This ledger records evaluated
candidates, including ones that have not been implemented. Runtime verification
and source inspection are different evidence levels.

## NP-001 — Native filesystem contract references (12340 forks)

- Status: independent native file/path implementation and synthetic fixtures pass
  on Linux x86-64, macOS ARM64 and Windows x64, including Linux sanitizers. M2
  package/async/asset/SDL integration followed in PRs #6–#8; see NP-002 and the
  [M2 handoff](m2/HANDOFF.md) for current evidence and acceptance limits.
- References: RosemyneH/wowdecomp `d91c854bc9c6af8d27e6e4093d76985671893bc5`,
  Arlyon/wowdecomp `24778cd77a6a217bbfdffccb88d9556a06b8b21d` (`macos`), both
  3.3.5a/12340 forks. Full source links and findings are in the
  [M2 reference review](m2/REFERENCE_REVIEW.md).
- Inspected contracts: File/Filestack open/read/write/metadata/move operations,
  `OsFile` adapters and Darwin alignment/no-replace filesystem leaves. Their
  POSIX/OS separation informed this design; implementation bodies are newly
  written, not imported, and have no donor binary-address/verification claim.
- Destination: `BlizzardCore/Portable/File*`, `Os/Portable/File.cpp`, explicit
  `WowNativeFiles` target and synthetic `Tools/PortableFiles` fixtures.
- Adaptation: M1 native-runtime ABI, opaque per-stream state, checked 64-bit I/O,
  deterministic case-ambiguity failure and explicit separate host roots. Existing
  Windows implementations remain selected for the full client.
- Assets/defaults: no game assets, network/schema changes, signature-check changes,
  addon redirects, renderer changes or optional effects. Native files are opt-in.
- Validation/limits: [filesystem design and evidence](m2/FILESYSTEM.md). No source
  inspection or native file fixture establishes gameplay or completes M2.

## NP-002 — Native package, worker and SDL3 shell contracts (12340)

- Status: independently implemented native boundaries merged in
  [PR #6](https://github.com/RovxBot/wowdecomp/pull/6),
  [PR #7](https://github.com/RovxBot/wowdecomp/pull/7) and
  [PR #8](https://github.com/RovxBot/wowdecomp/pull/8). Physical input acceptance
  and full game integration remain distinct; see the [handoff](m2/HANDOFF.md).
- References: the same RosemyneH and Arlyon 12340 commits as NP-001, plus this
  checkout's original `Client::OpenArchives`, `PatchFiles`, MPQ/SComp readers,
  signature verification, Services async queue, Os events and Windows input/IME
  callers. The forks informed POSIX/SDL boundaries; no implementation body or
  graphics subsystem was imported from either fork or the 15595 donor.
- Destination: `BlizzardCore/Portable/Package*`, `Assets*`, `Asset*`,
  `Os/Portable/ModelArena*`, `Services/Portable/AsyncFile*`, `ShellSettings*`,
  `Os/Portable/Window*`, `Tools/NativeAssets` and `Tools/NativeShell`.
- Adaptation: explicit little-endian 12340 decoding, original archive precedence,
  original-key RSA and selective MD5 policy, bounded signed-relative model arena,
  retained typed tickets with owner-thread completion, and SDL3 main-thread input.
  The native APIs do not claim legacy object layouts or original binary addresses;
  unchanged MPQ encryption and legacy Windows paths retain their provenance.
  Native `stpl.h` type diagnostics use standard `name()` behind guards; legacy
  `raw_name()` expressions remain intact. Existing Windows cursor fixes remain.
- Dependencies: SDL 3.2.28 at `7f3ae3d57459e59943a4ecfefc8f6277ec6bf540`,
  archive SHA256 `7a8347c770b90b33daac2352858ca03f9c9a2ccc8ce711054870361d1a6b32e5`;
  existing bundled zlib 1.2.2, bzip2 1.0.2, blast 1.1 and LZMA SDK 4.40.
  Sources/notices remain attributed. Native blast calls serialize its static
  first-use tables and always decompress `MPQ_FILE_IMPLODE`, matching the original
  caller even when encoded length is at least output length. Legacy codecs stay
  unchanged. Focused concurrent and encoded-size fixtures cover these fixes.
- Assets/defaults: unchanged external retail 12340 data; no new assets or shaders.
  No optional visual effect is enabled. Native targets are opt-in and keep their
  ABI separate. Required verification remains enabled. Trial/network-streaming
  installs are unavailable; no fake-success replacement is provided.
- Validation: [hosted identities](m2/shell-builds.json), [asset evidence](m2/ASSETS.md)
  and [desktop evidence](m2/shell-runtime.json). Linux release/ASan/UBSan, Windows
  x64 native ABI and actual macOS ARM64 fixtures pass. Linux original-data desktop
  execution is distinct from dummy-driver fixtures, physical input, Gx rendering
  and gameplay; none establishes the later Vulkan/Metal graphics decision.

## NP-003 — SDL display-library lifetime ownership

- Status: selected for the Linux M2 baseline in
  [PR #11](https://github.com/RovxBot/wowdecomp/pull/11), implementation
  `50a7d91a1794529da6bc094fcf58dc5435392033`. M2 acceptance uses the observed
  clean X11 profile; Wayland's external Fontconfig qualification stays explicit.
- Reference: pinned SDL 3.2.28 at
  `7f3ae3d57459e59943a4ecfefc8f6277ec6bf540`, unchanged archive SHA256
  `7a8347c770b90b33daac2352858ca03f9c9a2ccc8ce711054870361d1a6b32e5`.
  Its supported `SDL_X11_SHARED`, `SDL_WAYLAND_SHARED` and
  `SDL_WAYLAND_LIBDECOR_SHARED` CMake choices control dynamic loading. No donor
  function, source body, original binary address or verification claim is imported.
- Destination/adaptation: native window CMake sets normal ELF display dependencies
  for Linux. The source/graph audit keeps `SDLDesktopControl` free of engine code
  and the portable engine ABI, while allowing CMake build utilities. A separate
  `linux-window-dynamic-asan` preset retains the previous lifetime for comparison.
- Benefit/evidence: the dynamic SDL-only control reproduces the original X11
  50,184-byte report. Linked control and original-data shell pass unsuppressed
  X11 ASan/UBSan/leak checks, including eight cycles and settings relaunch. All
  remaining linked Wayland allocation groups map to Fontconfig and reproduce
  without an engine or renderer; they are retained as failures, not suppressed.
  [Design/control results](m2/DESKTOP_LIFETIMES.md) and
  [final closeout](m2/closeout.json) preserve exact build/runtime identities.
- Dependencies/defaults: required X11/extensions, Wayland/EGL/cursor, xkbcommon,
  libdecor and EGL libraries are now Linux loader dependencies even when a single
  display provider is selected. Windows/macOS selection, SDL version/notices,
  original assets, file verification, protocol and optional effects stay unchanged.
  No `dlclose` shim, forced font-cache cleanup or sanitizer suppression is added.
- Scope: library ownership for the diagnostic shell. It does not select a Gx,
  Vulkan/Metal renderer, establish Cocoa desktop support or alter game schemas.

## NP-004 — Native SDL3/Gx OpenGL context boundary

- Status: first M3 work package merged in [PR #13](https://github.com/RovxBot/wowdecomp/pull/13);
  validation and remaining integration are recorded in [the M3 handoff](m3/HANDOFF.md).
- References: this checkout's 12340 `CGxDeviceOpenGl` context attach/remove,
  presentation, capability initialization and `GlExt.cpp` ARB entry points;
  pinned SDL 3.2.28 at `7f3ae3d57459e59943a4ecfefc8f6277ec6bf540`.
  No donor source body, binary address or verification annotation is imported.
- Destination/adaptation: `Gx/Portable/GlContext*`, the native Os window owner,
  explicit `WowNativeGl` allowlist and `NativeGlCheck`. SDL owns the window and
  compatibility context on the main thread; GL objects/procedures expire before
  context/window teardown. The original WGL/legacy Windows Gx implementation
  remains separately selected. Native FBO support is required; WGL pbuffers and
  NV/ATI optional extension paths are not emulated.
- Assets/defaults: native probe is opt-in; unchanged 12340 external MPQs,
  original-key signaturefile and registered-MD5 checks remain enabled. Optional
  loading-bar texture display is not a GlueXML login screen. No enhancement
  asset, shader-version conversion, protocol or server-policy change is made.
- Validation: all eight PR workflows passed, including full Windows x64 client
  compilation and two Linux GL configurations with 14 CTest checks each. Exact
  head `2afd390c8fb9f1d7b478c915b5b9d736bcc234d9` rendered the original glue
  texture on RX 7900 XTX / Mesa 26.2.4 / Xwayland; release and three
  ASan/UBSan/leak-enabled original-data cycles exited 0 with empty stderr and
  zero native workers. The [context record](m3/context-validation.json) retains
  exact manifests/hashes and separates operator observations. Full Gx integration, original fonts/UI,
  native networking, authentication and observed world entry remain M3 work.

## NP-005 — Native original shader resource service

- Status: merged in [PR #17](https://github.com/RovxBot/wowdecomp/pull/17),
  implementation `543243c676f16b1a2c32af38d8b0910c73eaadbd`; see
  [shader contracts and caller audit](m3/SHADERS.md).
- Source: product 12340 at `0786df65a30135c833d573bfda7836cf5cff8d70`,
  `CGxDevice::IShaderLoad`, `CGxShader::Load`, OpenGL shader creation/patch/constants
  and Gxu font UI callers. No donor body, address or VERIFY claim is imported.
- Destination/adaptation: `Gx/Portable/ShaderPackage*` and `GlProgram*`, explicit
  native Gx allowlist and probe. Bounded little-endian metadata/permutations,
  caller-count validation, token-based local-to-env conversion, main-thread GPU
  ownership, rejected-upload cleanup and explicit constant limits. The legacy
  Windows object/hash/refcount ABI remains separately selected.
- Assets/defaults: unchanged original UI arbvp1/arbfp1 `0x10003` shaders read from
  authenticated external packages with registered MD5 policy. No shader conversion,
  optional effect, new dependency or server/protocol change. Model constant-range
  expansion, other dialects, font/GlueXML and full device integration remain open.
- Validation: [durable shader evidence](m3/shader-validation.json) records all eight
  PR workflows passing, full Windows x64 compilation, 15 CTest checks per Linux GL
  configuration, and exact-source RX 7900 XTX release/three clean sanitizer cycles.
  Original UI shader execution and loading-bar output are observed; original login
  UI and gameplay are not. On 2026-10-18 the operator reported all requested
  window controls passed without crashes or corruption on the selected shader
  probe; its matching clean-exit report is retained separately from automatic
  checks. Both shader members lack registered MD5 entries; original
  signaturefile/selective policy is preserved without claiming per-file MD5
  coverage.

## NP-006 — Native Gx render-target ownership

- Status: qualified in [PR #20](https://github.com/RovxBot/wowdecomp/pull/20),
  selected implementation `2eb6dd9ee61c15aa5b788a6f7856cdb2019e9b4c`, merged as `4d74dd0c5330f92c3fc282329fc071c61b23f62a`; see [target contracts/caller audit](m3/RENDER_TARGETS.md).
- Source: product 12340 at `6023d6a1901637f58507c4eac987e1e450cdaa53`,
  `CGxDevice::DeviceSetRenderTarget`, OpenGL FBO/texture ownership, target/face
  mappings and shared-model async cancellation callers. No donor/fork body,
  original binary address or VERIFY claim is imported.
- Destination/adaptation: `Gx/Portable/GlRenderTargets*`, the explicit native Gx
  allowlist and integrated shader probe. Immutable retained texture handles,
  atomic attachment pairs, explicit/implicit depth ownership, all cube planes,
  feedback checks, GPU size validation, binding/PBO restoration and incomplete-FBO
  rollback. Legacy per-slot state/projection and `CGxTex`/`CAsyncObject` contracts
  remain explicit integration work; no native ticket or inspection geometry is
  substituted for them.
- Assets/defaults: original `0x10003` UI shaders/loading-bar through unchanged
  authenticated external packages. No new assets, optional effects, dependencies,
  signature bypasses, protocol or server-policy changes. Full-client gate retained.
- Validation: [durable evidence](m3/render-target-validation.json) records all eight
  successful implementation PR workflows, full Windows x64 compilation, 16 CTest
  checks per Linux GL configuration, exact-source RX 7900 XTX release/sanitizer
  target and original-data shader cycles. ZIP/binary/notices hashes were verified;
  automatic cycles had empty stderr and no probe workers at shutdown. No new
  physical-input or Windows runtime observation is claimed. Original login UI and
  observed world entry remain open.

## NP-007 — Native Gx transforms and normalized raster state

- Status: qualified in [PR #21](https://github.com/RovxBot/wowdecomp/pull/21),
  selected implementation `a1c43f820448cbb0b0c0eb2fda2ab59216687415`; see
  [transform contracts/caller audit](m3/TRANSFORMS.md). Merged as `37bc74b492ce3f547d8b222676a7cc547e7727ab` after render-target
  PR #20. M3 remains open.
- Source: product 12340 at `840dc0fbda9206573a167a55f9051d3d6e3bfd38`,
  Gx transform/viewport entry points, OpenGL projection/model-view/scissor and
  Gxu UI constant callers. No donor body, address or VERIFY claim is imported.
- Destination/adaptation: `Gx/Portable/GlTransforms*`, unchanged scalar
  `CGxMatrixStack.cpp`, Tempest `C44Matrix.cpp`/`C33Matrix.cpp`, explicit native
  Gx allowlist, semantic GPU fixture and integrated original-UI shader probe.
  Preserve row-vector order, stack saturation, Z conversion, target Y/cull flip,
  normalized pixel viewport/scissor and the separate UI view/projection contract.
  The literal perspective compression branch remains explicit and unqualified
  against historical Windows scene failures.
- Assets/defaults: unchanged authenticated external 12340 data and original
  `0x10003` UI programs. No new asset/dependency, optional effect, signature bypass,
  protocol or server-policy change. Full legacy device/cache/texture/async and
  original fonts/login/network/world integration remain open.
- Validation: [durable evidence](m3/transform-validation.json) records all eight
  successful implementation PR workflows, 17 CTest checks in each Linux GL
  configuration, distinct Windows merge-checkout compilation and verified binary
  identities. Exact-source RX 7900 XTX release/ASan/UBSan/leak checks passed three
  target/transform cycles each and one/three original-data shader cycles, with
  empty stderr and zero probe workers. Initial enum and fresh matrix-mode failures
  and their corrections remain recorded separately. Local Python checks: 25 pass,
  one CMake selection check unavailable locally; all 26 ran in hosted Linux.
  No local compilation or new physical-input/gameplay acceptance.

## NP-008 — Native Gx buffers and vertex submission

- Status: qualified in [PR #22](https://github.com/RovxBot/wowdecomp/pull/22),
  selected implementation `42859739d5359cffc0b55f33ebfed789ff152dcd`, stacked
  on transform PR #21; merged as `fd0ae32545f6da42b4fa58572eb26953131324c7`; see [buffer contracts](m3/BUFFERS.md).
- Source: product 12340 at `2a92c398ef818e690c897ec5806ff4097a9fac3a`,
  CGxPool/CGxBuf, BufData/Lock/Unlock/Stream, original vertex descriptors and
  OpenGL attribute/index/Draw callers. No donor body, address or VERIFY claim.
- Destination/adaptation: `Gx/Portable/GlBuffers*`, explicit native allowlist,
  `GlBuffersCheck` and integrated buffered PCT/original UI shader probe.
  Retained individual allocations, staging commit/abort, sparse upload coverage,
  actual uint16 index validation, all original attribute type codes and private
  VAO binding isolation. Legacy pool/broker/device/cache/async ABI remains open.
- Assets/defaults: unchanged authenticated external 12340 data and original
  `0x10003` UI programs. No new dependency, optional effect, verification bypass,
  protocol or server-policy change. Native full-client gate retained.
- Validation: [durable evidence](m3/buffer-validation.json) records all eight
  successful PR workflows, 18 CTest passes per Linux GL configuration, separate
  Windows full-client merge checkout and verified ZIP/binary identities. Exact-source
  RX 7900 XTX release/sanitizer buffer, target and transform checks passed three
  cycles each, plus one/three original-data shader cycles, empty stderr and zero
  workers. Local Python checks: 27 passed, one CMake selection check unavailable;
  all 28 ran in hosted Linux. Initial synthetic attribute mismatch is preserved.
  No new physical-input, Windows gameplay, macOS graphics or server acceptance.

## NP-009 — Native sampled textures and font-atlas update formats

- Status: qualified in [PR #23](https://github.com/RovxBot/wowdecomp/pull/23),
  selected implementation `8e4fcde51d7962411d27c462655ad677f95aa23c`;
  see [texture contracts](m3/TEXTURES.md).
- Source: product 12340 at `fd0ae32545f6da42b4fa58572eb26953131324c7`,
  original OpenGL upload/filter/format tables and Gxu font atlas callbacks.
  No donor body, original binary address or VERIFY claim imported.
- Adaptation: extend retained `GlRenderTargets` storage and feedback ownership
  to ordinary texture formats, mips/cube faces, bounded subrectangles/row strides,
  original filter/wrap mappings and explicit anisotropy limits. The probe's
  original UI shaders consume owned sampled textures and buffered PCT geometry.
  Native handles are not legacy CGxTex/cache/callback or async ABI exports.
- Assets/defaults: unchanged authenticated 12340 data and `0x10003` UI shaders,
  no new dependency/asset/effect, protocol change or server-policy modification.
- Validation: [durable evidence](m3/texture-validation.json) records all eight
  passing PR workflows, 19 CTest checks per Linux GL configuration, full Windows
  compilation, verified artifacts and exact-source RX 7900 XTX release/sanitizer
  texture/buffer/target/transform and original-data shader cycles. Empty stderr,
  zero workers, no suppressions. Local Python checks: 29 passed, one unavailable;
  all 30 ran hosted. Synthetic atlas pixels remain separate from original font
  rasterization/login UI and native server gameplay.

## NP-010 — Native original font face and glyph preview

- Status: qualified at `16c111a5f36864530a2109ff7228a78f68fb17fd` in
  [PR #24](https://github.com/RovxBot/wowdecomp/pull/24); see [font contracts](m3/FONTS.md).
- Source: product 12340 at `365016e226547f7f3867f419988ccdf6b3182f25`,
  original memory face, glyph load/raster, font size/advance/kerning and atlas
  encoding/batched PCT callers. No donor code/address/VERIFY claim is imported.
- Adaptation: retained native `Gxu/Portable/FontFace*`, Storm-backed bundled
  TrueType/SFNT FreeType modules with native ABI, bounded owned coverage and thread affinity,
  original unhinted rendering/advance conventions and original-data UI preview.
  This is not legacy HFACE/CGxFont/cache/layout or complete login integration.
- Assets/defaults: unchanged authenticated `Fonts/FRIZQT__.TTF` and original UI
  shaders, no enhancement assets, new download, protocol or server-policy change.
  Existing FreeType attribution/license is retained in artifacts. CI uses a tiny
  authored geometric font, separate from retail data.
- Validation: [durable evidence](m3/font-validation.json) records nine passing PR
  workflows, 20 CTest checks per Linux GL configuration, full Windows x64
  compilation, verified artifacts and real-GPU release/strict ASan/UBSan/leak
  original-font/UI-shader checks. One/three original-data cycles and three cycles
  per resource fixture passed with empty stderr and zero workers. Prior FreeType
  callback, signed-scaling, empty-array and stream-lifetime failures remain
  recorded; native corrections preserve the Windows paths and all detectors.
  Native login/world, Windows runtime and macOS font/render coverage remain open.

## NP-011 — Native 12340 packet prerequisites

- Status: qualified at `6d1a9c209410d42f4351e71645c44edbe891cfb2` in
  [PR #25](https://github.com/RovxBot/wowdecomp/pull/25); see
  [packet contracts](m3/PACKETS.md) and [evidence](m3/packet-validation.json).
- Source: product main `365016e226547f7f3867f419988ccdf6b3182f25`, 12340
  `Base/CDataStore*`, `Storm/Source/SArc4.cpp`, BlizzardCore `Util/SHA1.cpp`
  and existing WowServices HMAC callers. No donor body/address is imported.
- Adaptation: native-only explicit little-endian/unaligned scalar operations,
  bounded buffer ownership, original scalar ARC4, incremental SHA1 without
  invalid input-pointer arithmetic and aligned digest compression scratch.
  Original OFFSET/VERIFY comments do not qualify these native adaptations.
- Assets/defaults: authored byte fixtures only; no data, credentials, protocol,
  signature-policy, server-policy or optional-effect change.
- Validation: all nine implementation PR workflows passed. Linux release and
  ASan/UBSan/leak, actual ARM64 macOS and Windows native configurations each
  passed five CTest checks/three packet cycles. Full Windows x64 compiled at
  separate merge checkout `9a2bcc9d247cb93361dcc3fc26bbc8c85dc5ddff` with
  signatures enabled. All five archive digests and 19 packaged binary hashes/types
  were verified. Local Python checks: 32 passed, one CMake check unavailable.
  No local compilation or sockets/login/gameplay observation.


## NP-012 — Native legacy Storm string prerequisites

- Status: qualified at `2d946465e52d7ccfb43722d844d76a4fa4cd038b` in
  [PR #26](https://github.com/RovxBot/wowdecomp/pull/26); see
  [contracts and caller limits](m3/LEGACY_STRINGS.md) and
  [durable evidence](m3/string-validation.json).
- Source: product 12340 main `5090a23e6fc46f572df301ac8f1a9a13b29c409b`,
  `Storm/Source/SStr.cpp`, `soprintf.cpp`, `SOption.cpp` and Gx/Gxu shader/font
  hash/cache callers. No donor code, address or new VERIFY claim is imported.
- Adaptation: explicit native core source selection, host-independent default
  byte case rules, fixed-width decimal limits, bounded native printf and ordered
  va-list handling, full argument/grammar preflight and ordinary fixed-width
  `I64` directive translation. Keep original hash/UTF-8 algorithms and actual
  TSHashTable allocation/key ownership; guard substantive adaptations with the native ABI.
  Original Windows bodies remain selected in the full client.
- Assets/defaults: no game data, enhancement, dependency or protocol/policy change.
  Native ordered UTF-16 wide formats remain visibly unavailable pending an
  encoding adapter; this is not the complete legacy device or original login UI.
- Validation: nine implementation PR workflows passed, including full Windows
  x64 compilation, actual ARM64 macOS and strict Linux sanitizer checks. All seven
  archive digests and 45 packaged binary hashes/types were verified. Exact-source
  RX 7900 XTX original-data font/UI-shader release/strict sanitizer regressions
  passed one/three cycles; each resource fixture passed three cycles in both modes.
  Core string/hash fixtures passed three local release and three strict sanitizer
  executions of downloaded binaries. Local Python checks: 32 passed, one CMake
  check unavailable. All compilation ran in Actions. Actual device/Gxu/login,
  transport and observed gameplay remain open.

## NP-013 — Native legacy Gx UI dispatch

- Status: implementation in [PR #27](https://github.com/RovxBot/wowdecomp/pull/27),
  originally stacked on PR #26; qualified at `6ef31f4a94cef0d1b1947f46e4e35d716568b520`.
  PR #26 merged as `c3ef9fd989d15beb4af2695ea0cc08865e23bd19`; PR #27 merged
  as `6dae38e729a44c8df2424a0f27d6b892fcac7b0f`.
- Source: product 12340 Storm-string handoff
  `7f4ca29b95379505f07ae74f4834ff5f99ec0693`. Select the existing `Gx.cpp`,
  `CGxDevice`, shader, texture/flags, pool/buffer, format/gamma/light and required
  scalar Tempest sources in the native ABI. No donor implementation/address,
  new dependency or new binary-verification claim is imported. Existing VERIFY
  annotations describe historical Windows bodies, not guarded native adaptations.
- Benefit/adaptation: concrete SDL-context-owned native UI device beneath actual
  Gx dispatch, original render-state stack/cache and resource objects. Typed owner
  markers preserve texture readiness without GLuint/pointer aliasing; callbacks
  retain borrowed bytes through exactly one Unlock after completed Lock. Native
  buffer retirement clears bindings before deletion; shader cache admission is
  bounded and handles partial permutation reuse. Native-only state comparison,
  scalar integer delivery and owner checks retain the original Windows paths.
- Assets/defaults: unchanged `0x10003` UI shaders and original font bytes remain
  behind authenticated original-key/registered-MD5 package reads on the retained
  worker. No enhancement, credentials, server traffic or policy change. This is
  a single-mip 2D/UI adapter, not original Gxu HFACE/layout/cache or login/world.
- Validation: all nine implementation PR workflows passed; both Linux GL
  configurations passed 21 CTest checks and full Windows x64 compiled at a separate
  matching-tree merge checkout. Three archive digests and 31 packaged binary
  hashes/types were verified. Exact-source RX 7900 XTX original-data release and
  strict ASan/UBSan/leak legacy dispatch/font/UI-shader checks passed three cycles
  each, with empty stderr and zero probe workers; existing preview/resource
  regressions passed too. Initial failures and exact identities remain in
  [the record](m3/legacy-device-validation.json). Local Python: 34 passed, one
  unavailable CMake check skipped. All compilation ran in Actions. Original Gxu
  layout/cache, login, transport and observed gameplay remain open.

## NP-014 — Original Gxu handles, layout and caches in the native ABI

- Status: qualified at `ade47622c9023fee38fce1b6346be2f8be51fce4` in
  [PR #28](https://github.com/RovxBot/wowdecomp/pull/28).
- Source: product 12340 main `6dae38e729a44c8df2424a0f27d6b892fcac7b0f`,
  following merged PRs #26/#27. Select original `GxuFontFace/Font/Main/String/Util`,
  `GxuFontTextureCache`, `GxuFontMiscClasses`, `IGxuFontGlyph`, Gxu transforms,
  Services `TextBlock` and required scalar Base/Tempest dependencies.
  No donor import, new original-address claim or binary revalidation. Existing
  annotations describe retained Windows bodies; native branches are adaptations.
- Benefit/adaptation: actual HFACE and HTEXTFONT/HTEXTBLOCK ownership, original
  layout/markup/wrapping, glyph/kerning/LRU/atlas and streamed batch consumers use
  the concrete native Gx device. Completed font reads remain owned through face
  teardown; native raster bounds/signed pitch and callback CTexture ownership
  preserve live source bytes. Native handles and caches reject foreign-thread
  mutations before changing refcounts or state. Initial drawable binding and
  original OpenGL `CGxDeviceOpenGl::ISetCaps` pixel/texel-center flags complete
  the direct-window font submission boundary. Three original pixel conversions
  retain their epsilon while rejecting unsafe integer conversions.
- Assets/defaults: unchanged original font and `0x10003` UI shader members,
  original-key authentication and selective registered MD5s. Separate authored
  geometric/Unicode-alias cache fixtures are CC0; they are not application assets.
  No enhancement, server request, policy change or credential handling.
- Validation: nine implementation PR workflows passed, with 22 CTest checks per
  Linux GL configuration and full Windows x64 compilation. Three archive digests
  and all 33 packaged binary hashes/types were verified. Exact-source RX 7900 XTX
  release and strict ASan/UBSan/leak runs passed three original-data Gxu/Services
  cycles and three authored eviction/regeneration cycles each. Prior Gx/preview/
  resource regressions passed. Local Python: 36 passed, one unavailable CMake
  check skipped. Scalar link, initial drawable and atlas sampling failures remain
  failures in the [record](m3/gxu-validation.json); all detectors stayed enabled. This is
  not original GlueXML/login, general Services image/async loading, full device/
  world coverage or observed gameplay. See [contracts](m3/LEGACY_GXU.md).

## NP-015 — Product-modified Lua runtime in the native ABI

- Status: qualified at `33ef88fdd742b6b2a4484f3c86c013642f977837` in
  [PR #29](https://github.com/RovxBot/wowdecomp/pull/29), merged as
  `f1726d9628bd15d09f8acee4442fff5976dac81f`.
- Source: product 12340 after qualified Gxu implementation `ade47622` and its
  closeout `a620c7b`, now merged in PR #28 as
  `d07e289755d303accd0f97d7d49bcfe21b84d538`. Explicitly select original
  `lua-5.1/src`, the product Storm pool and Base UTF-8 helpers. No donor import,
  dependency update or new original-address/VERIFY claim.
- Benefit/adaptation: preserve the product three-argument state creation,
  process-global taint, profile VM and addon memory/time APIs required by
  `FrameScript_Initialize`. Use signed 64-bit clocks, main executable-image
  callback validation on ELF/Mach-O/PE, native address membership and retained
  allocation ownership. Native number conversion, 32-bit parsing/formatting and
  x86 shift behavior avoid LP64/ARM64 drift and undefined C++ operations. Native
  integer-remainder overflow fails visibly. Original Windows bodies remain.
- Assets/defaults: asset-free authored fixtures; no host Lua or optional effect.
  Keep MPQ/TOC signatures, protocol and addon APIs. This does not select
  FrameScript/XML, load GlueXML/login, authenticate or enter the world.
- Validation: all ten implementation PR workflows passed; four native
  configurations passed six CTest checks and six Lua cycles each, including
  actual ARM64 and strict Linux ASan/UBSan/leak checks. Four archive digests,
  16 native executable hashes/types and original licenses were verified; the
  separate full Windows compilation archive and three PE x64 hashes were also
  verified. Downloaded Linux release/strict sanitizer execution passed six cycles
  each with expected allocation-overflow diagnostics and no sanitizer finding.
  Link, bytecode-fixture and inherited UB failures remain recorded. Local Python:
  38 passed, one unavailable-CMake skip. All compilation ran in Actions. See
  [contracts](m3/LUA.md) and [evidence](m3/lua-validation.json).

## NP-016 — Original FrameScript execution/events in the native ABI

- Status: qualified at `d49efe426d6fc590dc570b05d2bddb3bf4a36b38` in
  [PR #31](https://github.com/RovxBot/wowdecomp/pull/31), merged as `872ca9d962e73095a04ec7e5d816f27c59df35cb`.
- Source: product 12340 at `0cbf8a5f7464057848f140aebade9d16b966b812`, after
  merged Lua PR #29. Select original FrameScript/FrameScript_Object/script helpers
  and Base Status plus a native owner/provider. No 15595 import, dependency update
  or new original-address/VERIFY claim. Historical Windows bodies remain.
- Benefit/adaptation: actual original script/event/secure consumers use the modified
  Lua VM. Enforce single owner-thread lifecycle, finish execution/objects before
  reset, retain worker source bytes, normalize relative paths, bound reads and
  wrapper storage, preserve saved-variable closure taint and serializer, report
  failed/partial writes and defer nested listener changes to outermost dispatch.
  Native helper copies/conversions/stack-level bounds avoid hangs, overwrite and
  undefined conversions; error-handler mode recovers after a failing handler.
- Assets/defaults: authored fixtures in CI; external unchanged GlobalStrings only
  for local inspection. Original-key MPQ authentication/selective MD5 policy stay
  enabled. No TOC/interface signature claim, login/Frame/XML/image-service
  completion, protocol change, optional effect or server-policy change.
- Validation: all eleven PR workflows passed. Four native configurations passed
  12 CTest checks/six FrameScript cycles each, including actual ARM64 and strict
  Linux sanitizers. Five archives and 43 binary hashes/types were verified,
  including full Windows compilation at matching-tree checkout
  `95ef62bd82e4accbd1655f6e89fe50933e706369`.
  Downloaded Linux release/strict sanitizer binaries each passed six authored and
  six original-source cycles with empty stderr/zero workers. The first incorrect
  table-value taint fixture remains failed evidence; closure semantics are checked
  explicitly. Local Python: 40 passed, one unavailable-CMake skip. All compilation
  ran in Actions. See [contracts](m3/FRAMESCRIPT.md) and
  [durable evidence](m3/framescript-validation.json). M3 remains open.

## NP-017 — Original XML trees and interface signatures in the native ABI

- Status: qualified at `c79cd56a6854ab15e9cf89de47c4c2a3a7663b04` in
  [PR #33](https://github.com/RovxBot/wowdecomp/pull/33), merged as
  `c4ee06549c7f5cc5b6093a7d3189c8c6d1dac6bf`; M3 remains open.
- Source: this product's original 12340 `XMLTree.cpp`, Base allocator/interned
  strings/refcounts and bundled Expat 1.95.5; native adaptation of original
  `LoadXML.cpp` signature/manifest functions (`0x00816060`–`0x008165E0`) and
  original `CGlueMgr::InterfaceKey` (`0x009E2C28`). No 15595 donor import or
  new OFFSET/VERIFY claim; original Windows branches remain selected.
- Benefit/adaptation: owner-thread trees and retained interface reads share the
  qualified FrameScript lifecycle, with explicit tree/borrowed-node shutdown,
  pointer-aligned native intern entries and bounded parser/manifest input.
  Preserve raw depth-first digest order/duplicates, uppercase filename binding,
  original little-endian RSA and fail-closed incomplete traversal. Actual frame
  creation must still perform the original second digest comparison.
- Assets/defaults: unchanged read-only 12340/enUS data; original InterfaceKey and
  MPQ original-key/registered-MD5 policy stay enabled. No extra assets, dependency
  pins, optional effects, protocol or server-policy changes. Initial native XML
  excludes DTD and UTF-16/32. No actual Frame/GlueXML/login execution or rendering.
- Validation: all twelve implementation PR workflows passed, with 14 CTest
  checks/three authored cycles per native configuration, actual ARM64 and strict
  Linux sanitizers. Five archive digests/all 47 binary hashes/types verified,
  including full Windows x64 at matching-tree checkout
  `f7c2de912a17e55ef9196379b50c097c00dfbc8d`. Downloaded Linux release/sanitizer
  binaries each passed three authored and three original-data cycles, with empty
  stderr/zero workers. Both original interface digests authenticated and
  AccountLogin XML parsed. Prior compiler and restricted-executor leak-inspection
  failures remain in [evidence](m3/xml-validation.json); no detector was disabled.
  Local Python: 43 passed, one unavailable-CMake skip. All compilation in Actions.

## NP-018 — Authored native image mip chains and local readiness

- Status: qualified at `41c0a78e7f25bf6a040a59560c5f33502297d6f9` in
  [PR #37](https://github.com/RovxBot/wowdecomp/pull/37), merged as
  `0d90dcdd741e43418016aaeeb15a66c23be2f1f8`; see
  [contracts](m3/IMAGE_MIPS.md) and [evidence](m3/image-mip-validation.json).
  M3 remains open.
- Source: product main `9bcf7bc` after qualified image Services PR #35/closeout
  PR #36. Adapt original 12340 `CGxDeviceOpenGl::ITexMarkAsUpdated` mip callback
  traversal, `TextureGetGxTex` local wait-2 semantics and `CreateBlpAsync` UI read
  priority. No donor body, new address/VERIFY claim or dependency import.
- Adaptation: native immutable mip storage and retained complete authored BLP2
  levels; explicit mip filters, one Lock/Unlock around increasing Latch levels,
  dirty failure/retry and whole-chain updates. Retained pixels share a bounded
  budget. UI option 0x10 retains priority/duplicate cache ownership. Remote
  streaming, disk/atlas flags, automatic mip generation and incompatible sampler
  allocation changes remain visibly unavailable.
- Assets/defaults: unchanged authenticated original images and `0x10003` UI
  shaders, with separate authored level-color/callback fixtures. Original-key
  package authentication and registered MD5 checks remain enabled. No protocol,
  optional effect, server-policy or credential change; Frame/login remain open.
- Validation: all seven triggered implementation PR workflows passed, with 23
  CTest checks per Linux GL configuration and full Windows x64 compilation with
  signatures enabled. Three archive digests and all 35 packaged executable
  hashes/types were verified. Exact-source RX 7900 XTX release/strict sanitizer
  authored/original-data mip, Gxu/Gx/preview/resource executions passed: eighteen
  runs, empty stderr, zero probe workers. Original loading-bar captures match each
  other and prior qualification; original terrain mip checks are FBO telemetry,
  not world rendering. Local source audit: 43 passed, one platform skip. All
  compilation ran in Actions. PR #35 identities/failures remain historical.

## NP-019 — Original Frame layout/anchor ownership in the native ABI

- Status: qualified in [PR #39](https://github.com/RovxBot/wowdecomp/pull/39) at
  `4aa176506079a35e81c3bf9d7108e7bff1d57df2`, merged as
  `4cf5e0e536dca1bb96f06d8a8500e7ddd293c754`. See [contracts](m3/LAYOUT.md)
  and [durable validation](m3/layout-validation.json). M3 remains open.
- Source: product main `7add229c943a98ed5a038046eb4115e30fbf5043`, following
  image mip PR #37 and closeout #38. Select original 12340 CLayoutFrame/CFramePoint,
  coordinate/rectangle consumers, shared original LoadXML anchor/dimension and
  scalar comparison bodies. No donor body, new OFFSET/VERIFY claim or dependency.
- Adaptation: owner-thread XML/VM lease, object/point/pool accounting, initialized
  native flags/portable RTTI, unsigned anchor checks, finite dimensions/scales,
  bounded acyclic anchor graphs and transitive invalidation after origin destruction.
  Original solver, scaling/clamping/protection/deferred ordering remain selected;
  Windows bodies remain intact. Root factories/deferred destruction are next.
- Assets/defaults: authored layout/XML geometry. Original-data XML/signature
  regressions stay separate from original Frame construction or rendering. No data,
  protocol, asset enhancement, credential or server-policy change. Original package
  and InterfaceKey authentication/registered MD5 checks remain enabled.
- Validation: thirteen passing implementation PR workflows; four native layouts
  passed 15 CTest checks/three cycles each, including strict Linux sanitizers and
  actual ARM64 execution. Full Windows x64 compiled with signatures enabled.
  Five archives/all 51 binary hashes/types verified. Downloaded Linux layout and
  original-data XML/signature checks each passed three cycles per mode, exit 0,
  empty stderr. Prior missing-helper link and authored CRect field-order failures
  remain recorded. No new GPU/input/desktop/server or gameplay acceptance.

## NP-020 — Original Frame font/name factories in the native ABI

- Status: qualified in [PR #41](https://github.com/RovxBot/wowdecomp/pull/41) at
  `c87df98ab5b8f05cab034941dece92a78f81d726`, merged as
  `829904d849546a224201a885fdb4e1c55ce578e5`. See [contracts](m3/FRAME_FONTS.md)
  and [validation](m3/frame-font-validation.json). M3 remains incomplete.
- Source: product main `edeb6b3aaa2358e782635fdb0a84027f367aecaa`, following layout
  PR #39/closeout #40. Original 12340 font/name/Lua/XML consumers; no donor import,
  new dependency, asset enhancement or new OFFSET/VERIFY claim.
- Adaptation: owner leases, canonical live Lua tables/typed dispatch, bounded font
  inheritance/names, finite numeric inputs and callback-list lifetime guards.
  Native-only bundled FreeType general/X/Y movement bridges preserve original
  index conversion and Windows bodies. Existing fontable lists, override/dirty
  masks and Services/Gxu font validation remain selected. Signature defaults stay.
- Validation: thirteen passing implementation PR workflows, 31 checks per Linux
  GL configuration, full Windows x64 compilation; three archives and 45 packaged
  hashes/types verified. Downloaded release/strict ASan/UBSan/leak font/authored
  movement/original-data Gxu runs passed 18 cycles total, empty stderr/zero workers.
  Eighteen original GlueFonts definitions constructed; one selected outlined font
  rendered with matching exact-repeat/release/sanitizer pixels. Prior small-font
  expectation and incompatible FreeType callback failures remain recorded; the
  authored movement font reproduces the latter in the prior verified binary.
  Historical tiny/cache font bytes and all sanitizer detectors remain intact.
- Limits: actual regions/font strings/root, disk/atlas image policy, full GlueXML/
  login and gameplay remain open. Sample text/red tint are authored inputs. No
  new physical-input, Windows runtime, macOS graphics or server-session acceptance.

## NP-021 — Native Frame image lookup and atlas eligibility

- Status: qualified in [PR #43](https://github.com/RovxBot/wowdecomp/pull/43) at
  `e308d2e7eedc1e8f8cba7cfc8cec885e8fe5bfa6`, merged as
  `dbb4a00d8c85b74ad11f34a8a5f68aa91692aa1e`. See
  [contracts/next callers](m3/IMAGE_SOURCES.md) and
  [validation](m3/image-source-validation.json). M3 remains incomplete.
- Source: product main `c6ec88eb299671ce87240d8abf52bd80e5fa255d`, following Frame
  font PR #41/closeout #42. Original 12340 Services TextureCreate/PumpBlpTextureAsync,
  CTextureAtlas slot/coordinate/update behavior; no donor import, new dependency,
  asset enhancement or new OFFSET/VERIFY claim. Windows bodies remain selected.
- Adaptation: optional read-only loose-image root, source/representation-aware
  cache, actual marked single-level clamped atlas sheets with original 8 by 8
  coordinates and immediate uploads, shared pixel budget, GPU-before-pixel final
  teardown and default-filter policy. Native whole-sheet callbacks retain the
  qualified Gx contract; no legacy bit15 partial-source/async ABI is fabricated.
- Validation: thirteen passing workflows, 31 checks per Linux GL configuration,
  full Windows x64 compilation with signatures enabled. Three archives/45 packaged
  binary hashes/types verified. Downloaded release/strict sanitizer image and
  original GlueFonts checks passed eighteen cycles, empty stderr/zero workers.
  Loading-bar captures match the previous qualification. It is unmarked and stays
  standalone; shared-atlas/disk overrides are authored evidence. The first fixture
  queued-package read-count mistake remains failed evidence; ordering was corrected
  without implementation or detector changes.
- Limits: actual regions/font strings/root/Frame texture modes, broader atlas/mip
  sources, TGA/alternate lookup, streaming, full signed GlueXML/login and gameplay
  remain open. No new physical-input, Windows runtime, macOS graphics or server
  acceptance. Signatures/server discrepancy/interactive sign-in policy stays intact.

## NP-022 — Original base Frame XML factories and retained templates

- Status: qualified in [PR #70](https://github.com/RovxBot/wowdecomp/pull/70) at
  `2932c2fdb7eac99bc1a583f3c4374e7ab49fc71c`, merged as
  `70faf33a85e5a967530ea463d7da4f9d94fc7ba1`. See [contracts](m3/FRAME_XML.md)
  and [durable validation](m3/frame-xml-validation.json). M3 remains incomplete.
- Source: product main `4d8e7cef42f09b4ac91ccc73e6cfd5dd19664fa0`; original
  12340 FrameXML registry/template creation, SimpleFrameScript CreateFrame and
  Frame/FontString/title/layer XML consumers. No donor import, dependency upgrade,
  new OFFSET/VERIFY claim or game asset. Scalar XML helpers move into RegionXML.
- Adaptation: owner-bound registry and retained detached virtual nodes; original
  class pools/canonical Lua objects, named parents, original class-specific
  inheritance/taint, bounded graphs/numerics/names, exception-safe borrows and
  allocated-descendant rollback. Reject assignment-hook reentry. Native-only
  bundled FreeType empty point/tag/contour/extra copies skip null zero-byte libc
  arguments; a focused empty-loader regression covers the observed strict failure.
  Original Windows-selected code remains intact. Close consumers before registry/
  scripts/Fonts/layout/XML/VM/device/context/queue.
- Assets/defaults: authored XML/text through original or authored resources;
  original-key archives, registered MD5 and original GlueFonts-subset InterfaceKey
  checks remain enabled. No data/protocol/server-policy or credential change.
  Complete signed ordered GlueXML/full post-creation digest gate stays open.
- Validation: thirteen passing PR workflows, 38 CTest checks per Linux GL
  configuration, full signature-enabled Windows x64 compilation at matching-tree
  checkout `da814becf9a5a35681062ad14263e82cc07820c1`. Three archives/all 51
  binary hashes/types/notices verified. Downloaded RX 7900 XTX release/strict
  sanitizer packages passed 30 three-cycle executions, empty stderr/zero workers;
  fifteen capture pairs match. Earlier no-ink fixture/empty-outline UBSan failures
  remain recorded, without detector suppression. Local source/launcher checks:
  57 passed, one unavailable-CMake skip; source token/whitespace checks passed.
  All compilation ran in Actions. No new physical-input, Windows runtime,
  macOS graphics or server acceptance; full login and transport/gameplay remain open.

## NP-023 — Original Frame texture regions, XML and Lua consumers

- Status: qualified in [PR #72](https://github.com/RovxBot/wowdecomp/pull/72) at
  `6727ee12de9dd44a74dd548b63764efe35da0244`, merged as `519be5f497074a582419f0a9276e73d82ff69c3a`.
  See [contracts](m3/FRAME_TEXTURES.md) and [validation](m3/frame-texture-validation.json).
  M3 remains incomplete.
- Source: product main `839ff7a961572f0a71ffea9eec9b5362aabb2323`; original
  12340 CSimpleTexture, Frame/XML/Lua creation and retained render consumers.
  Texture bodies and scalar XML helpers move into explicit shared sources.
  No donor import, dependency upgrade, new binary-verification claim or game data.
- Adaptation: native-aligned original class pools and mixed heap/pool teardown;
  canonical Lua names/objects, exact-key retained templates and existing preflight;
  initialized native flags, raw mode/sampler validation, dirty coordinate/nonblocking
  edits and retained image/shader quad handles. Native 8x8 ARGB solid callbacks own
  pixels through GPU teardown, preserving opacity without the original global
  solid cache. Existing file/mip/atlas/read/wait contracts stay in effect.
- Assets/defaults: authored composition with original or authored loading-bar/UI/
  Desaturate resources; original-key archives, selective MD5 and GlueFonts-subset
  InterfaceKey checks stay enabled. Animation/tiling and other widgets remain open;
  no protocol/server-policy/credential changes or full login claim.
- Validation: thirteen passing implementation PR workflows; 38 CTest checks per
  Linux GL configuration and signature-enabled Windows x64 compilation at
  `a8c63b738b0b45081f7c71bb77b9e3182b6d9986`. Three archives/all 51 binaries/notices verified;
  30 three-cycle release/strict sanitizer desktop executions passed on RX 7900 XTX
  on 2026-10-24 with empty stderr/zero workers; fifteen final captures retain the
  prior baseline. Intermediate readbacks cover solid/file/atlas/desaturation/UV/
  alpha/visibility and XML/template consumers, with growth to 737 pool slots.
  Native missing-header and Windows formatter/table extraction failures remain
  evidence. Original Windows tokens and local source/launcher/whitespace checks
  passed. All compilation ran in Actions; physical IME/edit-box acceptance, full
  signed GlueXML/login and transport/gameplay remain open.

## NP-024 — Original Frame Lua and XML handlers

- Status: qualified in [PR #74](https://github.com/RovxBot/wowdecomp/pull/74) at
  `9d3384f9436aaa9c763fca0c35a8a91b1be734b9`, merged as
  `1cea05886d4fa4b68552f1615030e18ce7de111b`. See [contracts](m3/FRAME_HANDLERS.md)
  and [validation](m3/frame-handler-validation.json). M3 remains open.
- Source: product base `8ca66f5cc9be86f49b84ad7153b559d0ebb4065a`; original
  12340 Frame discovery/GetScript/SetScript/HookScript/event methods,
  LoadXML_Scripts and key/character/mouse/drag/update consumers. Original
  GetMouseButtonName moves unchanged into a shared source. No donor import,
  dependency update, new OFFSET/VERIFY claim or game asset; Windows tokens match.
- Adaptation: argument-3 ownership, matching/bounded compiled XML caches, guarded
  callback structural lifetimes, selected text edits and raw internal globals
  around protected calls. Canonical tables, taint/hook wrappers, pending listeners
  and retained strings remain selected. Close consumers before Scripts/templates,
  renderer/Fonts/Gxu/layout/XML/VM/device/context/queue.
- Validation: final-head GL release/strict sanitizer 38/38 checks; 12 prior passing
  workflows at `ac2b05f84ab00920244b987096ed59d923fd8e9e`, including full Windows
  signatures-enabled compilation at `a2e991897220e6ad51bea9193cc85866a7c06929`
  and ARM64 native prerequisites. Two GL fixtures/scheduling docs alone changed
  afterward; other 3,876 tracked files match. Three archives/all 51 executable
  hashes/types/notices verified. Downloaded RX 7900 XTX release/strict packages
  passed 30 three-cycle desktop runs, empty stderr/zero workers; fifteen capture
  pairs match. Distinct callback glyph pixels are intermediate readbacks; final
  captures retain authored hierarchy text. Local 57 pass/one CMake skip, token/
  extraction/whitespace pass; all compilation in Actions.
- Earlier nil/security, error-handler restore and obsolete mouse fixture failures,
  plus Actions-disabled dispatch, remain evidence; no product behavior/detector
  suppression. Trigger-only CI PR #75 removes duplicate branch push builds and
  exact-input reuse avoids repeat production matrices for fixture corrections.
  Original signatures and read-only clean data stay enabled. Remaining widgets,
  full signed GlueXML/login, physical IME/edit boxes and transport/gameplay remain
  open. No new physical-input, Windows runtime, macOS graphics or server acceptance;
  preserve server discrepancies and interactive sign-in.

## UP-001 — Reviewed 12340 upstream maintenance

- Status: merged in [PR #15](https://github.com/RovxBot/wowdecomp/pull/15), candidate
  `aac28edaf5463c15b9612ced9168fa67cbbe1607`, main merge
  `d1da8da29f575f4c57434cacab96664b82cef796`.
- Source: `SatyPardus/wowdecomp`, shared ancestor
  `4119c4b590ddd6934045b00f78b4b15ba39f7be2` through
  `b1c71b8bd108823bca952f16c6f83a7ad33b9648`, build 12340. This is not a 15595
  import. The [50-commit review](upstream/2026-10-18.md) and
  [record](upstream/2026-10-18.json) identify every commit and disposition.
- Benefit/scope: console height initialization, correct one-byte guild-bank
  charge decoding, unit transport override, rounded integral attack-power Lua
  values, and completed definition/constant cleanup across 127 existing files.
  Keep final corrections `56888bd` and `8bf90bc`. No implementation body is
  relabeled with a donor address; upstream verification annotations retain their
  provenance without a blanket new verification claim.
- Adaptation: a conflict-free history merge preserves all 195 fork-only changed
  paths byte-for-byte. The sole shared file, Event/Types.h, keeps native guards
  and adds historical definitions as comments. Native/legacy ABIs remain separate.
- Assets/defaults: unchanged 12340 data, signature defaults and shader format;
  no optional effect, extra asset, dependency pin or server-policy change. Packet
  charge width matches the inspected original PE and AzerothCore source writer.
- Validation: all eight hosted workflows passed, including full Windows x64
  compilation and existing native matrices; downloaded Windows binary identities
  match the manifest. Focused read-only original PE inspection supports the four
  behavior fixes. New Windows gameplay and native full-game acceptance remain open.

## UP-002 — 12340 upstream corrections and definitions, October 24

- Status: qualified at `4655b604` in [PR #77](https://github.com/RovxBot/wowdecomp/pull/77). See the
  [review](upstream/2026-10-24.md) and [record](upstream/2026-10-24.json).
- Source: `SatyPardus/wowdecomp`, previous upstream `b1c71b8b` through
  `20f9120e0a8f064338ecd9f2dd6cbc01642bf695`; 187 commits, 404 changed paths. This is product
  12340 maintenance; the 15595 donor pin is unchanged.
- Benefit: font-row packing, sky/talent/portrait/item-swap initialization, ID-based
  display lookups, FMOD shutdown lock ordering and completed named domains.
  Preserve corrective follow-ups and final raw-field/width/sentinel behavior.
- Adaptation: resolve eight conflicting files, keeping native ownership guards
  and moved shared functions. Carry enum names into extracted helpers and
  fixtures; retain signatures, protocol, dependency pins and original assets.
  Generated legacy version headers use full Git history; packaging remains
  Actions-only. Source addresses and prior VERIFY annotations retain provenance.
- Validation: 57 local source/launcher tests pass, one CMake-unavailable skip;
  no local compilation. Original clean PE inspection supports font-row/sky/talent/portrait
  details. Five selected hosted suites passed, including strict GL sanitizers,
  actual ARM64 prerequisites and signature-enabled full Windows compilation.
  Five archives/70 binary hashes/types/notices verified. RX 7900 XTX release/strict
  sanitizer packages passed 32 three-cycle desktop executions, empty stderr/zero
  workers; 16 capture pairs and 15 historical comparisons match. Preserve the
  record's older layout/shell checkouts and both retained compile failures.
  Actual Windows gameplay/audio, original native login, macOS graphics and server compatibility remain unobserved for this batch.

## BP-001 — Correct CPUID structure initialization

- Status: source-confirmed candidate; not applied or runtime-tested during M0.
- Product revision: `cd29da38b9162419ee0fa29c6b16e07e10f6c4f0`.
- Product function: `System_Hardware::ReadCPUID`, tagged 12340 address `0x0042E120`,
  in [Hardware.cpp](../Engine/Source/BlizzardCore/Source/System/Hardware/Win64/Hardware.cpp).
- Observed defect: `memset(info, sizeof(info), 0)` supplies a zero byte count and
  therefore does not initialize the output structure. `sizeof(info)` is also the
  pointer size, not the structure size. This conclusion follows from the call's
  arguments; it is not a demonstrated gameplay failure.
- Donor: [wowdecomp15595 at `959b0b85c5f55c565328d7c3ca33027a65e41327`](https://github.com/SatyPardus/wowdecomp15595/blob/959b0b85c5f55c565328d7c3ca33027a65e41327/Engine/Source/BlizzardCore/Source/System/Hardware/Win64/Hardware.cpp),
  same function, tagged 15595 addresses `0x007AD1B0` (x86) / `0x1404A2EB0` (x64).
- Relevant donor behavior: `memset(info, 0, sizeof(CPUIDInfo))`. The rest of its CPU
  detection algorithm and dependencies differ and should not be imported wholesale.
- Intended adaptation: an independent initialization fix with a focused check for
  initialized fields/early-return behavior, plus Windows Actions compilation. Keep
  portable/ARM64 CPU detection as a separate M1/M5 task.
- Assets/protocol/defaults: no asset changes, packet changes or feature flag needed.
- Supported backends: CPU-only fix; graphics backend independent.
- Evidence needed before landing: isolate output initialization, check early-return
  behavior without requiring a specific host CPU, preserve/refresh verification
  annotations, run the Windows baseline build in Actions and record results.
- Disposition: queued for a small implementation change after the M0 inventory.
