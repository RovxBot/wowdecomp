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
  selected implementation `2eb6dd9ee61c15aa5b788a6f7856cdb2019e9b4c`, awaiting
  merge; see [target contracts/caller audit](m3/RENDER_TARGETS.md).
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
  [transform contracts/caller audit](m3/TRANSFORMS.md). Stacked on qualified,
  unmerged render-target PR #20; both remain unmerged and M3 remains open.
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

- Status: implemented for hosted validation, stacked on qualified, unmerged
  transform PR #21; see [buffer contracts/caller audit](m3/BUFFERS.md).
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
- Validation: hosted compilation, GPU fixtures and actual-GPU qualification pending.
  No new physical-input, Windows gameplay, macOS graphics or server acceptance.

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
