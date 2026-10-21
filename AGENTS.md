# Project instructions for coding agents

## Mission and reading order

Build a native 3.3.5a (12340) client for Linux x86-64 and macOS ARM64, retaining
Windows x64 as the regression reference. Use 4.3.4 (15595) as a reference and source
of selected improvements. Read [PORTING_PLAN.md](PORTING_PLAN.md) and the relevant
module/build files before changing implementation. [README.md](README.md) describes
the Windows client and hosted native targets; the full native game is still gated.

Follow the user's current scope and instructions. The plan guides implementation;
it does not turn a documentation request into authorization to start the whole port.

## Current handoff and build policy

- All client and dependency compilation must run in GitHub Actions, including
  experimental build checks. Do not build the client locally. Source inspection,
  documentation checks and source-audit Python tests may run locally.
- Use [Windows baseline](.github/workflows/windows-baseline.yml) for the existing
  x64 reference and [M0 source audit](.github/workflows/m0-audit.yml) for comparison
  against the pinned donor. Add Linux/macOS Actions jobs as their targets become
  buildable; do not claim unsupported native builds from the current workflows.
- M0 source/dependency inventory and Windows x64 compilation are verified. Operator
  Windows sessions report D3D9 passes and matching pre-existing OpenGL failures;
  data/server identity and captured evidence remain partial. Read [docs/m0/BASELINE.md](docs/m0/BASELINE.md), the
  [durable build manifest](docs/m0/windows-baseline.json), the
  [runtime session record](docs/m0/RUNTIME_SESSION.md) and
  [backport ledger](docs/BACKPORTS.md) before continuing.
- M0 [PR #1](https://github.com/RovxBot/wowdecomp/pull/1) and initial M1
  [PR #2](https://github.com/RovxBot/wowdecomp/pull/2) were merged into `main` on
  2026-10-17. Preserve the reports' exact historical run/commit identities.
- The M0 product source is `cd29da38b9162419ee0fa29c6b16e07e10f6c4f0`, including the
  upstream tutorial bounds fix after the original planning revision. Preserve it.
- Treat `/mnt/games/ChromieCraft` as read-only. It is a modified 12340/enUS data
  candidate with case-colliding MPQs and proxy/patch components; do not delete or
  rename them to manufacture a stock baseline. Establish clean data separately.
- The user supplied `/mnt/games/3.3.5aClean` as a separate clean reference. Treat it
  as read-only too. Its 12340 executable and 18 enUS MPQs are identified in
  [the clean-data inventory](docs/m2/clean-data-inventory.json). Native Linux
  inspection authenticated the original signaturefile and decoded representative
  original assets with ASan/UBSan and leak detection enabled; see
  [asset evidence](docs/m2/ASSETS.md). This does not verify every archive as stock
  or establish server/gameplay compatibility. Keep settings/cache outside both
  installations and keep game data out of Git and ordinary hosted CI.
- GrimGuzzler is the target server. Local deployment sources identify AzerothCore
  with playerbots and enabled Warden; desired manifests do not verify live image
  identity, realm endpoints or gameplay compatibility. Do not change cluster/server
  policy as part of the client port.
  A later [read-only live snapshot](docs/m3/SERVER_IDENTITY.md) records runtime
  image IDs, matching packaged/PID-1 executable hashes, embedded world version,
  mounted Warden and listener metadata externally. The embedded revision differs
  from the package tag and has a `+` suffix; do not substitute that tag or a local
  checkout for an exact clean compiled source tree. Realm-list endpoint, client
  Warden responses and native gameplay remain unobserved. Historical M0 sessions
  retain their own incomplete server identities.
- M1 runtime foundations were merged into main via
  [PR #3](https://github.com/RovxBot/wowdecomp/pull/3), with passing hosted core,
  native lifecycle, sanitizer and Windows client checks. M1 core exit criteria
  are met. Read [docs/m1/FOUNDATIONS.md](docs/m1/FOUNDATIONS.md),
  [runtime design](docs/m1/RUNTIME_DESIGN.md) and the
  [upstream review](docs/m1/UPSTREAM_REVIEW.md) before continuing.
- M0 remains partial until complete data/server evidence and the required Windows
  observations/captures are recorded. M1 core success does not establish gameplay.
- M2 host files [PR #5](https://github.com/RovxBot/wowdecomp/pull/5), authenticated
  archives/asset inspection [PR #6](https://github.com/RovxBot/wowdecomp/pull/6)
  and native async files [PR #7](https://github.com/RovxBot/wowdecomp/pull/7) are
  merged. Read the [filesystem work package](docs/m2/FILESYSTEM.md),
  [asset contracts/evidence](docs/m2/ASSETS.md), [async contracts](docs/m2/ASYNC.md)
  and [native-reference review](docs/m2/REFERENCE_REVIEW.md) before continuing.
  SDL3 shell [PR #8](https://github.com/RovxBot/wowdecomp/pull/8) is also merged
  (implementation `2154bed3917ab1322545a365a58c5d2e708906b3`, merge
  `6eecb78e99c889e6f97e55ce9fe5d5855d95ab89`). Read the current
  [M2 handoff](docs/m2/HANDOFF.md), [shell contracts](docs/m2/SHELL.md),
  [hosted identities](docs/m2/shell-builds.json) and
  [desktop runtime evidence](docs/m2/shell-runtime.json). Original-data Linux
  startup/settings/shutdown are observed on X11 and Wayland. The operator confirmed
  the requested physical-input checklist passed on 2026-10-18; the saved final
  Wayland report corroborates real mouse/clipboard activity and clean shutdown.
  See [operator acceptance](docs/m2/operator-acceptance.json). IME language and
  additional display/platform coverage were not identified. M2 exit criteria are
  met for the observed Linux X11 asset/shell baseline as of 2026-10-18. Lifecycle
  [PR #11](https://github.com/RovxBot/wowdecomp/pull/11) selected implementation
  `50a7d91a1794529da6bc094fcf58dc5435392033`, merged as
  `df0f2d0f1f7abf86afd125f9d4d42d37ce542625`. Read the
  [closeout evidence](docs/m2/closeout.json) and
  [desktop lifetime design](docs/m2/DESKTOP_LIFETIMES.md). Exact-source X11 release,
  ASan/UBSan/leak checks, settings relaunch and repeated SDL teardown passed.
  Wayland's remaining Fontconfig report reproduces without engine code or a
  renderer; retain it as a system-library qualification, not a clean sanitizer
  pass. Preserve
  the distinction between operator observations, telemetry and injected fixtures.
  Preserve the distinction between scalar inspection geometry and full
  animation/material/world rendering.
- M3 context [PR #13](https://github.com/RovxBot/wowdecomp/pull/13) selected head
  `2afd390c8fb9f1d7b478c915b5b9d736bcc234d9`, merged as
  `d1dd70aedeae7f09468692503590c4fdbc9423ff`. Read the
  [M3 handoff](docs/m3/HANDOFF.md) and [context evidence](docs/m3/context-validation.json).
  All eight PR workflows passed, including full Windows x64 compilation; both
  Linux GL configurations passed 14 CTest checks. Exact-head release and three
  ASan/UBSan/leak-enabled context/asset cycles passed on AMD RX 7900 XTX,
  SDL X11 on Xwayland, with original 12340/enUS signature authentication and
  loading-bar texture display. The new Gx host context is not yet wired into the
  legacy device/native ABI adaptation. Original login UI, portable networking,
  authentication, character selection and observed GrimGuzzler world entry remain
  open; M3 is not complete. On 2026-10-18 the operator confirmed resize,
  minimize/restore, focus, F11 and close passed on the matching synthetic probe,
  without blank graphics or crashes. Boxes scaled with the window. Keep those
  operator observations separate from automatic GPU telemetry and game rendering.
- M3 original shader service [PR #17](https://github.com/RovxBot/wowdecomp/pull/17)
  selected head `543243c676f16b1a2c32af38d8b0910c73eaadbd`, merged as
  `7c5f5ec4feb59760a25aec07d30dbaa433826da5`. Read the
  [shader contracts](docs/m3/SHADERS.md) and
  [durable validation](docs/m3/shader-validation.json). All eight PR workflows
  passed, including full Windows x64 compilation; both Linux GL configurations
  passed 15 CTest checks. Exact-source release and three ASan/UBSan/leak-enabled
  cycles executed original `0x10003` UI shaders on RX 7900 XTX / Xwayland, checked
  five FBO pixels and displayed the original loading-bar texture, with empty
  stderr and zero workers. Both UI shader members lack registered MD5 entries;
  original-key signaturefile authentication and selective MD5 policy remain
  enabled. This native resource does not expose legacy `CGxShader` objects.
  Keep its original 94-register range and unsupported dialects explicit.
  Preserve its main-thread/current-context ownership and distinguish uploaded
  stereo permutations from rendered stereo, original shaders from original
  fonts/GlueXML, and rendered pixels from gameplay. Full native device/targets,
  login UI/network/world remain open; M3 is not complete. On 2026-10-18 the
  operator confirmed resize, minimize/restore, focus changes, F11 and window close
  all passed on this exact original-data shader probe, with no crash or corruption.
  The matching release report corroborates exit 0, empty stderr and zero workers;
  it has no per-event telemetry or new capture. Keep this operator acceptance
  separate from automatic GPU/sanitizer checks and historical context acceptance.
- M3 [render-target PR #20](https://github.com/RovxBot/wowdecomp/pull/20) selected
  implementation `2eb6dd9ee61c15aa5b788a6f7856cdb2019e9b4c` is merged as
  `4d74dd0c5330f92c3fc282329fc071c61b23f62a`. Read the [target contracts](docs/m3/RENDER_TARGETS.md) and
  [validation](docs/m3/render-target-validation.json). All eight implementation
  PR workflows passed; both Linux GL configurations passed 16 CTest checks and
  the full Windows x64 client compiled at separate merge checkout
  `30a0acafc0af371f9bb1f77ee6360585ece00cbf`. Exact-source RX 7900 XTX / Mesa
  26.2.5 / SDL X11 release and ASan/UBSan/leak checks passed three target cycles
  each, plus one release and three sanitizer original-data shader cycles, with
  empty stderr and zero native workers at each probe shutdown. Targets retain
  attachment/sampler storage, reject feedback and roll back incomplete FBOs.
  The zero-width image failure is an explicit fixture injection. This is not
  legacy `CGxTex`/device/state/projection/async integration, original fonts/login
  UI or gameplay. No new physical-input acceptance is recorded; keep prior
  operator observations attached to their historical sources. M3 remains open.
- M3 transform [PR #21](https://github.com/RovxBot/wowdecomp/pull/21) selected
  implementation `a1c43f820448cbb0b0c0eb2fda2ab59216687415` is merged as
  `37bc74b492ce3f547d8b222676a7cc547e7727ab`, following PR #20. Read [transform contracts](docs/m3/TRANSFORMS.md)
  and [validation](docs/m3/transform-validation.json). All eight implementation
  PR workflows passed; both Linux GL configurations passed 17 CTest checks and
  full Windows x64 compilation used separate merge checkout
  `b18ed2af2622a8dd15e67a5863a9997b1f143f13`. Exact-source RX 7900 XTX / Mesa
  26.2.5 / SDL X11 release and ASan/UBSan/leak checks passed three target and
  transform cycles each, plus one release and three sanitizer original-data
  shader cycles, with empty stderr and zero probe workers. Scalar CGxMatrixStack
  and Tempest math, normalized raster state and original UI shader constants are
  integrated into the native probe. Initialize the legacy GL_MODELVIEW baseline
  before querying/restoring matrix mode; validate unsigned stack indices before
  enum interpretation. Nonidentity texture-matrix delivery fails visibly pending
  integration. Keep the literal bit-31 perspective-compression branch and its
  unqualified Windows warped-scene behavior explicit. Full legacy device/cache/
  texture/buffer/async, original fonts/login UI, networking and observed gameplay
  remain open. No new physical-input acceptance or server session is recorded;
  preserve the external tag/embedded-revision distinction. M3 is not complete.
- M3 buffers [PR #22](https://github.com/RovxBot/wowdecomp/pull/22) selected
  implementation `42859739d5359cffc0b55f33ebfed789ff152dcd` is merged as
  `fd0ae32545f6da42b4fa58572eb26953131324c7`, following PR #21. Read [buffer contracts](docs/m3/BUFFERS.md) and
  [validation](docs/m3/buffer-validation.json). All eight implementation PR
  workflows passed; both Linux GL configurations passed 18 CTest checks and
  full Windows x64 compilation used separate merge checkout
  `827d3cdd0876014389239e3ae40eea8d053e6a5e`. Exact-source RX 7900 XTX / Mesa
  26.2.5 / SDL X11 release and ASan/UBSan/leak buffer, target and transform checks
  each passed three cycles, plus one release and three sanitizer original-data
  shader cycles, with empty stderr and zero probe workers. Retained native
  vertex/index buffers now submit PCT triangles through a private VAO. Preserve
  original generic attribute indices/types; synthetic shaders must explicitly
  read Gx attributes 4/6 rather than assume conventional color/texcoord aliases.
  Staging writes retain allocations until commit/abort; draw bounds include
  actual uint16 indices and uploaded byte coverage. This does not export legacy
  CGxPool/CGxBuf/device/cache/async objects or complete fonts/login, networking
  or gameplay. No new physical-input/server acceptance is recorded; preserve the
  external tag/embedded-revision distinction. M3 remains open.
- M3 texture [PR #23](https://github.com/RovxBot/wowdecomp/pull/23) selected
  implementation `8e4fcde51d7962411d27c462655ad677f95aa23c` is merged as
  `365016e226547f7f3867f419988ccdf6b3182f25`.
  Read [texture contracts](docs/m3/TEXTURES.md) and
  [validation](docs/m3/texture-validation.json). All eight PR workflows passed;
  both Linux GL configurations passed 19 CTest checks, and full Windows x64
  compilation used separate merge checkout `dc4bde6d308abb9d36d3b6d8cc29f5e2552319e5`.
  Exact-source RX 7900 XTX release/sanitizer texture, buffer, target and transform
  checks each passed three cycles, plus one/three original-data shader cycles,
  with empty stderr and zero workers. Sampled textures share attachment/sampler
  ownership and feedback checks, accept bounded mip/face/rectangle uploads and
  isolate unpack/PBO state. ARGB4444 atlas pixels are synthetic, not original
  glyph rasterization. Original font/login, device/cache/callback/async and
  networking/world work remain open. The user will sign in interactively when
  native login is ready. Preserve server revision discrepancies. M3 is incomplete.
- M3 packet prerequisites [PR #25](https://github.com/RovxBot/wowdecomp/pull/25)
  are qualified at `6d1a9c209410d42f4351e71645c44edbe891cfb2`. Read
  [packet contracts](docs/m3/PACKETS.md) and [validation](docs/m3/packet-validation.json)
  before extending transport. All nine implementation PR workflows passed;
  Linux release and ASan/UBSan/leak, actual ARM64 macOS and Windows native builds
  each passed five CTest checks/three packet cycles. Full Windows x64 compiled at
  separate merge checkout `9a2bcc9d247cb93361dcc3fc26bbc8c85dc5ddff` with
  signatures enabled; all five archive digests and 19 packaged binary hashes/types
  were verified. This adapts original `CDataStore`, ARC4 and SHA1/HMAC in the native
  ABI, not socket dispatch or authentication. M3 remains incomplete; preserve the
  external server revision discrepancy and interactive user sign-in preference.
- M3 original-font [PR #24](https://github.com/RovxBot/wowdecomp/pull/24) is
  qualified at `16c111a5f36864530a2109ff7228a78f68fb17fd`, merged as
  `5090a23e6fc46f572df301ac8f1a9a13b29c409b`. Read
  [font contracts](docs/m3/FONTS.md), [validation](docs/m3/font-validation.json)
  and the [legacy Gx audit](docs/m3/LEGACY_GX.md). Nine PR workflows passed;
  Linux release/strict sanitizer configurations passed 20 CTest checks each.
  Full Windows x64 compilation used separate merge checkout
  `528822b509acfb57869cb65591edea824ddd09e9`. Exact-source RX 7900 XTX / Mesa
  26.2.5 / SDL X11 release and strict ASan/UBSan/leak checks passed one/three
  original Friz Quadrata font/UI-shader cycles and three cycles of each resource
  fixture, with empty stderr and zero workers. Native-only bundled FreeType
  callback/scaling/empty-array/stream cleanup corrections retain prior failures
  and the original Windows paths. An authored font program covers size-time
  interpreter execution even with unhinted glyphs. Original HFACE/Gxu layout/cache,
  global legacy device/async integration, actual login, networking and gameplay
  remain open. No new operator acceptance/server session; M3 is incomplete.
- M3 Storm string/cache [PR #26](https://github.com/RovxBot/wowdecomp/pull/26)
  is qualified at `2d946465e52d7ccfb43722d844d76a4fa4cd038b`, merged as
  `c3ef9fd989d15beb4af2695ea0cc08865e23bd19`. Read
  [string contracts](docs/m3/LEGACY_STRINGS.md) and
  [validation](docs/m3/string-validation.json). Nine implementation PR workflows
  passed, including full Windows x64 compilation; actual ARM64 macOS, Windows and
  Linux native fixtures passed. Seven archive digests and 45 packaged binary
  hashes/types were verified. Original-data GPU font/UI-shader release/strict
  sanitizer regressions passed one/three cycles with empty stderr and zero workers.
  Preserve original hashes, native format argument preflight/ownership and ordinary
  `I64` integer values without enabling global ordered formatting. Ordered UTF-16
  formats remain visibly unavailable. Actual legacy device/Gxu/login, transport
  and observed gameplay remain open; M3 is incomplete.
- M3 legacy Gx UI dispatch [PR #27](https://github.com/RovxBot/wowdecomp/pull/27)
  is qualified at `6ef31f4a94cef0d1b1947f46e4e35d716568b520`, originally stacked
  on PR #26 and merged as `6dae38e729a44c8df2424a0f27d6b892fcac7b0f`.
  Read [device contracts](docs/m3/LEGACY_DEVICE.md) and
  [validation](docs/m3/legacy-device-validation.json). Nine implementation PR
  workflows passed, with 21 CTest checks per Linux GL configuration and full
  Windows x64 compilation at separate matching-tree checkout
  `d319ec4bef5ed042ce8b45fb62923b5f927ecd60`. Three archive digests and 31 packaged binary
  hashes/types were verified. Exact-source RX 7900 XTX / Mesa 26.2.5 / SDL X11
  release and strict ASan/UBSan/leak checks each passed three original-data
  callback/PCT/font/UI-shader cycles; existing preview/resource regressions also
  passed, with empty stderr and zero probe workers. Concrete native CGxDevice now
  owns actual global Gx state/cache/resources through the qualified native
  services. Preserve owner-thread mutations, typed readiness markers, exactly
  one Unlock after completed Lock, partial shader-permutation cache repair and
  derived-before-base GPU/binding teardown. Keep native draw-list key destructors
  so UBSan vptr checks remain enabled. This is a bounded single-mip 2D/UI adapter,
  not original Gxu HFACE/texture-service/layout/string/glyph-cache integration or
  full world-device coverage. Original Gxu/login, transport/authentication and
  observed gameplay remain open. No new physical-input or server acceptance;
  preserve the external server revision discrepancy. M3 is incomplete.
- M3 original Gxu [PR #28](https://github.com/RovxBot/wowdecomp/pull/28) is
  qualified at `ade47622c9023fee38fce1b6346be2f8be51fce4`, following merged
  PRs #26/#27. Read [Gxu contracts and next callers](docs/m3/LEGACY_GXU.md) and
  [validation](docs/m3/gxu-validation.json). Nine implementation PR workflows
  passed, with 22 CTest checks per Linux GL configuration and full Windows x64
  compilation at separate matching-tree checkout
  `ca7e229e536c5434c3e9c6d26f6c8bf03494eb6a`. Three archive digests and all 33
  packaged binary hashes/types were verified. Exact-source RX 7900 XTX / Mesa
  26.2.5 / SDL X11 release and strict ASan/UBSan/leak execution passed three
  original-data Gxu/Services text-handle cycles and three authored cache-pressure
  cycles each, with empty stderr and zero workers. Prior Gx/preview/resource
  regressions passed. Original HFACE, HTEXTFONT/HTEXTBLOCK, layout/markup/kerning,
  glyph/row/string caches and batched consumers now use the native Gx adapter.
  Preserve font bytes through FT_Done_Face, owner-thread handle/cache mutation,
  fonts until text-block destruction, original OpenGL pixel/texel centers and
  original rounding epsilon. Close consumers before the font owner/device/context.
  The original Friz face has no glyphs in the tested stress range; eight-page
  eviction/regeneration is separate authored evidence. This does not qualify all
  styles/languages/stereo/world-space rendering, general Services image/async
  loading, original Frame/Lua/GlueXML/login or gameplay. No new physical-input,
  Windows runtime or macOS graphics acceptance; keep signatures and server
  revision discrepancies intact. The user will sign in interactively when ready.
- M3 product Lua [PR #29](https://github.com/RovxBot/wowdecomp/pull/29) is
  qualified at `33ef88fdd742b6b2a4484f3c86c013642f977837`, merged as
  `f1726d9628bd15d09f8acee4442fff5976dac81f`, following original Gxu
  [PR #28](https://github.com/RovxBot/wowdecomp/pull/28), now merged as
  `d07e289755d303accd0f97d7d49bcfe21b84d538`. Read the
  [Lua contracts and next callers](docs/m3/LUA.md) and
  [validation](docs/m3/lua-validation.json). All ten implementation workflows
  passed; Linux release/strict ASan/UBSan/leak, actual ARM64 macOS and Windows
  native fixtures each passed six CTest checks and six Lua cycles. Five archives,
  all 19 packaged binary hashes/types and Lua licenses were verified. Full
  Windows x64 compiled at separate matching-tree PR checkout
  `3c48f75f4e9bbe94a4e5c09c612fb9c88652acb3` with signatures enabled.
  Downloaded exact-source Linux release/strict sanitizer executions passed six
  cycles each with six expected allocation-overflow diagnostics and no sanitizer
  finding. Retain the original modified VM, Storm pool, taint/accounting/profile
  APIs and five libraries; do not substitute host Lua. Native callback validation
  admits main-image executable ranges only. Preserve Windows numeric widths,
  original x86 shift behavior and source-only loading: exported `lua_dump` does
  not imply bytecode loading. Null upvalue terminators and table bounds have
  native UB corrections; original Windows paths and historical failure evidence
  remain. Application owner affinity, original FrameScript file/event/secure
  handlers, Frame/XML/GlueXML, image/async services and login remain open. No new
  GPU, physical-input, Windows runtime, macOS desktop or server acceptance is
  recorded. M3 is incomplete; signatures, server revision discrepancies and
  interactive user sign-in preference remain unchanged.
- M3 original FrameScript [PR #31](https://github.com/RovxBot/wowdecomp/pull/31)
  is qualified at `d49efe426d6fc590dc570b05d2bddb3bf4a36b38`, merged as
  `872ca9d962e73095a04ec7e5d816f27c59df35cb`. Read [FrameScript contracts](docs/m3/FRAMESCRIPT.md)
  and [validation](docs/m3/framescript-validation.json). All eleven implementation
  PR workflows passed; Linux release/strict ASan/UBSan/leak, actual ARM64 macOS
  and Windows native configurations passed 12 CTest checks and six FrameScript
  cycles each. Five archives and all 43 packaged binary hashes/types were
  verified, including the full Windows x64 client at matching-tree checkout
  `95ef62bd82e4accbd1655f6e89fe50933e706369`, with signatures enabled. Downloaded
  Linux release/strict sanitizer binaries each passed six authored cycles and
  six unchanged original `GlobalStrings.lua` cycles with empty stderr and zero
  workers. Original script execution/events/secure handlers and saved-variable
  serialization now use an owner-thread VM and completed-read provider. Close
  objects/external function references and finish executions before reset/close.
  Nested listener changes commit at the outermost dispatch. Preserve the original
  SavedVariables closure taint, distinct from table/scalar-value taint; the first
  fixture's mistaken expectation remains failed evidence. The source member has
  no registered MPQ MD5 entry; original-key authentication and selective MD5 policy
  remain enabled. Original TOC/interface signatures, Frame/XML/image services,
  GlueXML/login rendering, transport and gameplay remain open. No new GPU,
  physical-input, Windows runtime, macOS desktop or server acceptance. Preserve
  server revision discrepancies and interactive user sign-in. M3 remains open.
- M3 original XML/signatures [PR #33](https://github.com/RovxBot/wowdecomp/pull/33)
  is qualified at `c79cd56a6854ab15e9cf89de47c4c2a3a7663b04`, merged as
  `c4ee06549c7f5cc5b6093a7d3189c8c6d1dac6bf`. Read [XML contracts/next callers](docs/m3/XML.md)
  and [validation](docs/m3/xml-validation.json). All twelve implementation PR
  workflows passed, with 14 CTest checks/three authored cycles per native
  configuration, actual ARM64 execution and full Windows x64 compilation at
  matching-tree checkout `f7c2de912a17e55ef9196379b50c097c00dfbc8d`.
  Five archives and all 47 packaged executable hashes/types were verified.
  Downloaded Linux release/strict ASan/UBSan/leak binaries each passed three
  authored and three original-data cycles with empty stderr/zero workers.
  Original InterfaceKey authenticated GlueXML (60 visits, MD5
  `b44247235136edcfbbc314797eb2c705`) and FrameXML plus Bindings (267 visits, MD5
  `51b264400410d3409c4e79f6f4257711`); AccountLogin XML parsed. Preserve native
  pointer alignment, borrowed nodes until tree close, XML-before-VM shutdown,
  raw manifest order and the second digest comparison after actual frame creation.
  The initial native tree dialect rejects DTD and UTF-16/32; it is not general
  addon/language qualification. Compiler failures and the restricted-executor
  LeakSanitizer ptrace failure remain recorded; the same binary passed outside
  tracing with all detectors unchanged. Frame factories/image Services and actual
  GlueXML/login execution/rendering remain open, followed by transport/gameplay.
  No new GPU, physical-input, Windows runtime, macOS desktop or server acceptance;
  M3 stays open, signatures/server discrepancies/interactive sign-in remain intact.
- M3 file-backed image Services [PR #35](https://github.com/RovxBot/wowdecomp/pull/35)
  is qualified at `db24f90dc2d845d5a5944df0cedb46fd116d13ec`, merged as
  `b1479843dd7eac430514fc1103a4e9c1e6e39301`. Read [image contracts/next callers](docs/m3/IMAGES.md)
  and [validation](docs/m3/image-validation.json). Twelve implementation PR workflows
  passed; both Linux GL configurations passed 23 CTest checks. Full Windows x64
  compiled with signatures enabled at matching-tree checkout
  `b3df743c8c009d1076d9ec4acfbe3cbd6ba50e86`. Three archive digests and all 35
  packaged executable hashes/types were verified. Exact-source RX 7900 XTX / Mesa
  26.2.5 / SDL X11 release and strict ASan/UBSan/leak binaries each passed three
  authored and three original-data image cycles; Gxu/Gx/preview/resource regressions
  passed with empty stderr and zero workers. Original loading-bar capture hashes
  match. CTexture cache/handle/GPU mutation belongs to the owner; reads/decode use
  the retained worker. Close file handles before service/device/context/queue;
  retired completions cannot write into recreated entries. Native tickets remain
  separate from CAsyncObject. Preserve literal native status copies and original
  8x8/power-of-two limits, offscreen Y and CImVector BGR conventions. Initial
  authored fixture failures remain recorded. This mip-zero BLP2 surface accepts
  explicit nearest/linear sampling; Frame 0x17/wait-2, atlas/TGA/streaming and actual
  Frame factories/GlueXML/login remain open. No new physical-input, Windows runtime,
  macOS graphics or server acceptance. M3 remains open; signatures, server revision
  discrepancies and interactive user sign-in remain intact.
- M3 authored image mips/readiness [PR #37](https://github.com/RovxBot/wowdecomp/pull/37)
  is qualified at `41c0a78e7f25bf6a040a59560c5f33502297d6f9`, merged as
  `0d90dcdd741e43418016aaeeb15a66c23be2f1f8`. Read [mip contracts/next Frame callers](docs/m3/IMAGE_MIPS.md)
  and [validation](docs/m3/image-mip-validation.json). All seven triggered PR
  workflows passed; both Linux GL configurations passed 23 CTest checks. Full
  Windows x64 compiled with signatures enabled at matching-tree checkout
  `6c2521c2a897f9e4d192404b118139f784b7e22b`. Three archive digests and all 35
  packaged executable hashes/types were verified. Exact-source RX 7900 XTX / Mesa
  26.2.5 / SDL X11 release and strict ASan/UBSan/leak binaries each passed three
  authored and three original-data image cycles, including original 256x256 Elwynn
  authored mip-chain minification through Services/Gx/UI shaders. Gxu/Gx/preview/
  resource regressions passed; all eighteen executions exited 0 with empty stderr
  and zero probe workers. Loading-bar capture hashes match prior qualification;
  no new terrain/world capture is claimed. Preserve one Lock/Unlock around
  increasing Latch levels, complete authored chains, dirty retry, immutable mip
  allocation classes, local-only wait 2 and UI priority 129/ordinary 130. Retained
  pixels share a 256 MiB budget. Frame 0x17 disk/atlas flags remain rejected;
  actual Frame layout/factories, TGA/streaming, original GlueXML/login, transport
  and gameplay remain open. No new physical-input, Windows runtime, macOS graphics
  or server acceptance. M3 remains incomplete; signatures, server revision
  discrepancies and interactive user sign-in stay intact.
- M3 original layout [PR #39](https://github.com/RovxBot/wowdecomp/pull/39) is
  qualified at `4aa176506079a35e81c3bf9d7108e7bff1d57df2`, merged as
  `4cf5e0e536dca1bb96f06d8a8500e7ddd293c754`. Read [layout contracts/next callers](docs/m3/LAYOUT.md)
  and [validation](docs/m3/layout-validation.json). Thirteen implementation PR
  workflows passed; Linux release/strict ASan/UBSan/leak, actual ARM64 macOS and
  native Windows each passed 15 CTest checks/three layout cycles. Full Windows
  x64 compiled with signatures enabled at matching-tree checkout
  `8e4de17077fd39972148f7b0aa1101a28e711dd6`. Five archive digests and all 51
  packaged binary hashes/types were verified. Downloaded Linux release/strict
  sanitizer layout and original-data XML/signature checks each passed three cycles
  with empty stderr; XML workers reached zero. Original CLayoutFrame/CFramePoint
  solver, resize lists and absolute/relative XML dimensions now have native owner
  and pool lifetime, initialized flags, unsigned index checks, graph bounds and
  transitive failed-rectangle invalidation. Close objects outside resize callbacks
  before layout, XML and VM owners. Preserve original CRect Y/X order and scalar
  comparison epsilon; prior link/fixture failures remain recorded. This is authored
  layout, not original-data Frame layout/rendering. Actual font/string/region/root
  factories, disk/atlas/default image policy, GlueXML/login and transport/gameplay
  remain open. No new GPU, physical-input, Windows runtime, macOS desktop or server
  acceptance. M3 stays open; preserve signatures/server discrepancies/interactive
  user sign-in.
- M3 original Frame font/name factories [PR #41](https://github.com/RovxBot/wowdecomp/pull/41)
  are qualified at `c87df98ab5b8f05cab034941dece92a78f81d726`, merged as
  `829904d849546a224201a885fdb4e1c55ce578e5`. Read [font factory contracts/next callers](docs/m3/FRAME_FONTS.md)
  and [validation](docs/m3/frame-font-validation.json). Thirteen implementation PR
  workflows passed; both Linux GL configurations passed 31 CTest checks. Full
  Windows x64 compiled with signatures enabled at matching-tree checkout
  `ba54f09734f163123155e378048cadf3bcfde61b`. Three archive digests and all 45 packaged
  binary hashes/types were verified. Exact-source RX 7900 XTX / Mesa 26.2.5 /
  SDL X11 release and strict ASan/UBSan/leak checks each passed three authored
  movement/font-factory, three original GlueFonts and three original-data Gxu
  cycles, with empty stderr/zero workers. Eighteen original font definitions were
  constructed; one selected small outlined font rendered with exact repeat pixels
  and matching release/sanitizer captures. The sample text/red tint are authored.
  Native typed FreeType movement callbacks preserve original index conversion and
  Windows paths; retain the prior strict failure, authored reproducer and pixel
  expectation failure. Font/name pointers are borrowed; close external script
  objects/fontables/text blocks/handles before factories, then Gxu/layout/XML/VM/
  device/context/queue. Preserve canonical live Lua tables and update-list guards.
  Actual regions/font strings/root, SDL size invalidation, disk/atlas images and
  complete FrameXML/GlueXML/login remain open. No new physical-input, Windows
  runtime, macOS graphics or server acceptance; M3 stays incomplete. Preserve
  signatures, server revision discrepancies and interactive sign-in.
- Native packages support read-only retail MPQ v0/v1 data. Keep original-key RSA
  authentication and registered file MD5s enabled; the signaturefile prints four
  uint32 MD5 words in the original little-endian conversion convention. The
  synthetic fixture key must never replace the application factory's 12340 key.
- Native file requests retain ownership through exactly one owner-thread callback.
  Cancellation of a current read is cooperative. Do not destroy a queue from its
  callback or another thread, free its buffers early, or substitute its native
  ticket API for legacy CAsyncObject without adapting that caller's contract.
- SDL3 3.2.28 is pinned in the native window target. All SDL operations and event
  delivery belong to the main thread; asset reads belong to the retained worker.
  Native IME events own composition/result snapshots. Keep logical window and
  pixel sizes distinct and release held input/capture on focus loss. The shell
  uses SDL software rendering with GLES2/Metal framebuffer presentation where
  required; this is not the M3/M4 Gx or bgfx prototype. Optional vendor SDKs stay
  excluded and visibly unavailable. Native settings remain separate from WTF.
- Preserve upstream `4119c4b` / `5b41f72` cursor anchoring in both Windows Input.cpp
  files, in addition to the original tutorial bounds fix. The recorded M0/M1
  Windows gameplay sessions predate that input change. Linux SDL acceptance does
  not runtime-validate those Windows implementations.
- The 50 reviewed 12340 upstream commits through
  `b1c71b8bd108823bca952f16c6f83a7ad33b9648` were integrated by
  [PR #15](https://github.com/RovxBot/wowdecomp/pull/15), candidate head
  `aac28edaf5463c15b9612ced9168fa67cbbe1607`, merge
  `d1da8da29f575f4c57434cacab96664b82cef796`. Read the
  [upstream review](docs/upstream/2026-10-18.md) and
  [commit/build record](docs/upstream/2026-10-18.json). All eight workflows passed;
  Windows compilation used separate PR merge checkout
  `88dc92441c2ceac5957888371cd06dee4cf1c275`. Keep `56888bd`'s final attack-power
  rounding helper and `8bf90bc`'s archive behavior correction. Historical 0.5.3
  names without a current mapping stay comments. Original PE/static source
  findings are not new Windows gameplay or native login/world observations.

- M3 image lookup/atlas [PR #43](https://github.com/RovxBot/wowdecomp/pull/43)
  is qualified at `e308d2e7eedc1e8f8cba7cfc8cec885e8fe5bfa6`, merged as
  `dbb4a00d8c85b74ad11f34a8a5f68aa91692aa1e`. Read [image-source contracts/next callers](docs/m3/IMAGE_SOURCES.md)
  and [validation](docs/m3/image-source-validation.json). Thirteen PR workflows
  passed; both Linux GL configurations passed 31 CTest checks. Full Windows x64
  compiled with signatures enabled at matching-tree checkout
  `e520213854a27c5fb2c694e3effbb0b9e3443821`. Three archive digests and all 45
  packaged binary hashes/types were verified. Exact-source RX 7900 XTX / Mesa
  26.2.5 / SDL X11 release and strict ASan/UBSan/leak checks each passed three
  authored image, three original-data image and three original GlueFonts cycles,
  with empty stderr/zero workers. Loading-bar captures match prior qualification;
  the original BLP lacks the atlas flag and remains standalone. Shared-atlas and
  loose-source pixels are authored evidence. Native Services accept Frame 0x17
  eligibility, explicit read-only loose roots and default filtering. Marked square
  sources up to 64 use original 8 by 8 atlas coordinates with single-level clamped
  nearest/linear sampling; mip/repeat and other ineligible sources stay standalone.
  Preserve source/cache distinction, immediate cell uploads, shared pixel budget
  and GPU-before-pixel teardown. Prior fixture queued-read failure remains recorded.
  Actual regions/font strings/root, Frame texture modes, TGA/streaming, full
  signed GlueXML/login and gameplay remain open. No new physical-input, Windows
  runtime, macOS graphics or server acceptance; M3 remains incomplete. Preserve
  signatures, server revision discrepancies and interactive sign-in.

## Compatibility requirements

- Preserve 12340 authentication, protocol, packet layouts, object fields, gameplay,
  Lua APIs, addon loading and original asset compatibility by default.
- Keep unchanged 3.3.5a data sufficient for the baseline client. Track any optional
  enhancement assets separately and document their supported formats.
- Keep existing file/signature verification defaults. Portability work is not a
  reason to disable checks, suppress server requirements or hide failures.
- Preserve working Windows x64 behavior while adding native targets. Retain Win32
  comparison/layout support where practical; label unavailable checks explicitly.
- Target actual ARM64 execution on Apple silicon. An Intel macOS build or translated
  executable does not establish native ARM64 support.
- Introduce visual upgrades separately from the portability baseline. Optional
  effects start disabled and must leave original behavior intact when disabled.

## How to use the two repositories

The product baseline is this checkout. The initial donor is
[`SatyPardus/wowdecomp15595` at `959b0b85c5f55c565328d7c3ca33027a65e41327`](https://github.com/SatyPardus/wowdecomp15595/tree/959b0b85c5f55c565328d7c3ca33027a65e41327).
The audited local baseline and source findings are in the plan. Keep reference
checkouts outside the product tree; reference commits explicitly.

- Port individual fixes or cohesive features. Do not merge entire donor subsystems
  merely because the source is newer or has higher address-tag coverage.
- Before importing code, identify callers, data layouts, shader/material contracts,
  dependencies and the exact benefit for the 3.3.5a client.
- Record donor repository/commit, original build/address, paths/functions,
  adaptation, asset needs, feature defaults and validation in a backport ledger
  at [docs/BACKPORTS.md](docs/BACKPORTS.md). Preserve upstream attribution.
- Preserve `OFFSET`, `STATIC` and `VERIFY` provenance accurately. Do not present a
  donor address as a 12340 address or carry a verification claim onto changed code
  without revalidation. Annotate substantive adaptations with their source build.
- Remember that shader formats differ: local 3.3.5a uses version `0x10003`, while
  the inspected donor uses `0x10004`. Handle conversion explicitly.
- The donor is also Windows-only. Its D3D11 implementation and WGL OpenGL path are
  references, not completed native Linux/macOS backends.
- Keep Cataclysm JAM networking, gameplay schemas and object-field changes outside
  the 12340 baseline unless the user explicitly expands the compatibility scope.

## Implementation rules

- Work through the plan's dependencies and keep changes small enough to review.
  Keep platform refactors, bug fixes, donor imports and optional effects distinct.
- Prefer the existing `Os`, Storm, BlizzardCore and Gx boundaries. Audit all of
  them: porting only `Engine/Source/Os` will leave Windows dependencies elsewhere.
- Use explicit platform source selection and target-scoped CMake definitions and
  libraries. Do not define `WIN32` on other operating systems to force compilation.
- Separate OS, CPU and pointer width. `WOWDECOMP_ARCH_64` must not imply Windows,
  x86-64, SSE or the Windows ABI on ARM64/macOS/Linux targets.
- Keep Windows headers and handles in platform implementations where possible;
  expose opaque state or portable interfaces to common engine code.
- Use fixed-width types and explicit serialization for disk/network formats. Audit
  `long`, `wchar_t`, alignment, packing, varargs and member-pointer assumptions.
  Do not globally force native runtime structures to the original 32-bit layout.
- Preserve the M2 signed 32-bit relative-offset constraint. `M2Shared` uses
  `OsAllocNear` on x64; replacing it with arbitrary allocations can invalidate model
  data. Record the addressing/ownership design before changing that path.
  The native inspection model uses a separate bounded owner arena containing both
  relative-array anchors and payloads; it does not replace legacy M2Shared.
- Preserve startup dependencies, allocator/log availability during static
  initialization, floating-point behavior and adequate stack capacity. Inspect
  the custom Windows entry before introducing a native entry point.
- Preserve synchronization semantics, callback ordering/thread affinity, partial
  I/O, cancellation, timeouts and shutdown lifetime guarantees. Avoid replacing
  SEH or Windows events with superficially similar behavior without checking callers.
- Distinguish MPQ-internal paths from filesystem paths. Preserve patch/locale
  precedence and account for case sensitivity and writable settings/cache paths.
- Start ARM64 CPU work with correct scalar implementations. Add acceleration based
  on profiling, with scalar parity checks and explicit architecture guards.
- Treat SDL3 and a Gx/bgfx adapter as the plan's defaults to validate. Run the
  representative graphics/shader prototype before committing to a full backend
  migration; document a failed assumption and select an alternative with evidence.
- Keep audio mixing/spatial behavior behind `SE2`. A working device API alone is
  not a complete replacement for FMOD. Native targets cannot link Windows `.lib`
  or depend on Windows DLL loading.
- Keep third-party changes narrow, preserve notices, pin versions and document
  dependency/platform limitations. Add no game data, credentials or build outputs
  to source control.
- Do not make optional peripheral integrations mandatory for native startup. Label
  unavailable capabilities; do not substitute silent success stubs for required
  gameplay, graphics, audio or network behavior.

## Validation and reporting

Use checks appropriate to the behavior changed. Core portability and asynchronous
code need focused semantic tests; rendering needs scene comparisons on real
backends; ARM64 claims need hardware execution. Small documentation changes need
reference/whitespace checks and no client rebuild.

The current Windows baseline commands are executed by the Actions build script,
on its Windows runner only:

```powershell
cmake -S . -B build/x64 -G "Visual Studio 17 2022" -A x64
cmake --build build/x64 --config Release --target WowClient Wow --parallel
```

The full-client CMake configuration still rejects Linux and macOS. The M1 core
presets explicitly disable the client and build only allowlisted hash/core-service
sources and asset-free native lifecycle checks. Use [M1 core Actions](.github/workflows/m1-core.yml) for that
subset; its success establishes a minimal core lifecycle, not full client startup
or gameplay. Do not
invent presets/tests or claim full native support from core-only compilation. Add
native client build/run instructions when the corresponding Actions targets work.

M2 file, asset and async presets/workflows are separate allowlisted native ABI
targets, with synthetic semantic checks on Linux x86-64, macOS ARM64 and Windows
x64 plus Linux sanitizers. [M2 asset Actions](.github/workflows/m2-assets.yml)
produces `NativeAssetCheck`; execute downloaded, hash-verified binaries against
external data with [the runtime wrapper](Tools/Porting/run_asset_inspection.py).
This local execution is permitted; local compilation remains prohibited.
[M2 shell Actions](.github/workflows/m2-shell.yml) builds `NativeShell` with
`linux-window`, `linux-window-asan`, `macos-arm64-window` and `windows-window`.
Use [the shell runtime wrapper](Tools/Porting/run_native_shell.py) for hash-verified
execution with external data and writable user roots; see SHELL.md for commands.
An explicit Linux video-driver request also sets matching child EGL platform;
timeouts are recorded failures, not physical-input or clean-shutdown evidence.
Dummy-driver fixtures do not establish desktop input/IME, usable windows or Gx
rendering. Actual ARM64 fixtures do not establish Cocoa desktop/gameplay support.
Physical input acceptance and the selected X11 M2 baseline are complete. Report
unavailable hardware/session coverage explicitly. The SDL-only
desktop control must stay free of engine/data dependencies. Linux now links its
required display libraries with normal ELF process lifetime ownership: both the
SDL-only control and original-data shell passed X11 ASan/UBSan/leak checks. Keep
`linux-window-dynamic-asan` as a diagnostic comparison. The remaining Wayland
Fontconfig report reproduces without engine code and is retained, not suppressed;
do not claim a clean Wayland LeakSanitizer run.

For implementation changes, record the commands and checks run, their results and
any skipped platform checks. Update milestone status only with evidence. Distinguish
source inspection, compilation, fixture tests, rendered output and observed
gameplay; preserve honest gaps when hardware, data or a server is unavailable.

Use synthetic or small redistributable fixtures in ordinary CI. Keep full game data,
credentials and environment-specific paths external. Record the tested client/data
build, locale, server revision/settings, platform, architecture and GPU/toolchain
for gameplay and visual comparisons. The limited current x64 Warden handler means
server compatibility must be observed, not assumed.

Complete authorized, reversible work without routine confirmation requests. When an
external prerequisite is missing, continue independent work and state the specific
validation gap. Do not mark a milestone complete because its code compiles or its
remaining checks need another machine.

- M3 original Frame render batching [PR #45](https://github.com/RovxBot/wowdecomp/pull/45)
  is qualified at `d09ad319696fa7a05767bcc2af5f810af4378b3d`, merged as
  `d4b213b01e9a9d50a538c50f3247614930058517`. Read [batch contracts/next callers](docs/m3/FRAME_RENDER.md)
  and [validation](docs/m3/frame-render-validation.json). All thirteen implementation
  PR workflows passed; both Linux GL configurations passed 32 CTest checks. Full
  Windows x64 compiled with signatures enabled at matching-tree checkout
  `08dbdbc46ee4b5b0100aeaa427facc731d81332c`. Three archive digests/all 47 packaged
  binary hashes/types verified. Exact-source RX 7900 XTX / Mesa 26.2.5 / SDL X11
  release and strict ASan/UBSan/leak binaries each passed three authored batch,
  three original-data batch, three original GlueFonts, three image and three Gxu
  cycles, plus three cycles of each resource fixture, with empty stderr/zero workers.
  Preserve quad snapshots, retained texture/shader/font/block handles, block-before-
  font close, single intrusive string-batch membership, full-width sorting and
  original merge thresholds/atlas UVs. Finish draws before callback/list mutation;
  unwind only staging writes acquired by the batch. Close batches before renderer,
  Gxu and device/context. Original Windows bodies and all three earlier fixture
  failures remain recorded, with vptr and other detectors enabled. Captures match
  release/sanitizer; the authored composition uses original resources but is not
  original Frame objects/login. Actual Frame queues/regions/font strings, parent/
  animation/layer/root and deferred lifetimes, SDL size invalidation, texture modes
  and full signed ordered GlueXML plus post-creation digest gate remain open, then
  transport/gameplay. No new physical-input, Windows runtime, macOS graphics or
  server acceptance. M3 stays open; preserve signatures/server discrepancies and
  interactive user sign-in.

- M3 original unparented FontString regions [PR #47](https://github.com/RovxBot/wowdecomp/pull/47)
  are qualified at `01ac7deb547aa3216a92d78f93b6eb07e59eb9a0`, merged as
  `259518fd512a7c9cf4f4dcd8a4e0b0998058f282`. Read [region contracts/next callers](docs/m3/FRAME_REGIONS.md)
  and [validation](docs/m3/frame-regions-validation.json). Thirteen workflows
  passed; both Linux GL configurations passed 33 checks and full Windows x64
  compiled with signatures enabled at matching-tree checkout
  `c71a91a609e2a301415cd46e81cfff84b826613e`. Three archive digests and all 49
  packaged executable hashes/types verified. Exact-source RX 7900 XTX / Mesa
  26.2.5 / SDL X11 release and strict ASan/UBSan/leak binaries each passed three
  authored region, three original-data region and three each of Frame batch,
  GlueFonts, image and Gxu cycles; all four resource fixtures also passed three
  cycles per mode, with empty stderr and zero workers. Actual unparented C++
  regions now consume original layout/font attributes/Services and retained
  batches. Preserve block-before-font cleanup, lower-handle retention across
  consumer destruction, native field initialization, logical-pixel rounding,
  callback/layout guards and preflight rejection before Lua stack/taint/name
  mutation. Process pending layout after shadow-offset changes before batching;
  the two earlier fixture failures and strict unselected-class RTTI failure
  remain recorded without disabling detectors or changing pixel requirements.
  Original GlueXML/font definitions are consumed in authored text/anchor/scale
  composition, not login layout. Screen callbacks are injected, not SDL event
  acceptance. Actual Frame parent/layer/root, pooled factories, animation,
  region Lua/XML/language and texture/embedded consumers, full signed ordered
  GlueXML plus post-creation digest gate and original login remain open, followed
  by transport/gameplay. No new physical-input, Windows runtime, macOS graphics
  or server acceptance. M3 stays open; preserve signatures, server discrepancies
  and interactive user sign-in.

- M3 original Frame parent/layer/root [PR #49](https://github.com/RovxBot/wowdecomp/pull/49)
  is qualified at `ac717d347e2226e06a4f36e0a2bd485f396f6ef2`, merged as
  `9b01385d6a1bdb11a78905dd9c302d559cbc142d`. Read [hierarchy contracts/next callers](docs/m3/FRAME_HIERARCHY.md)
  and [validation](docs/m3/frame-hierarchy-validation.json). Thirteen workflows
  passed; both Linux GL configurations passed 34 checks and full Windows x64
  compiled with signatures enabled at matching-tree checkout
  `960ebca2d4bb7a50a7cd8ddd53f3e44a89f02274`. Three archive digests and all 51
  packaged executable hashes/types verified. Exact-source RX 7900 XTX / Mesa
  26.2.5 / SDL X11 release/strict ASan/UBSan/leak binaries each passed three
  authored and three original-data hierarchy cycles, plus region/batch/GlueFonts/
  image/Gxu and all four resource regressions, with empty stderr/zero workers.
  Captures match. Actual Frame parents/layers/strata/root now render authored
  text/layout through original resources. Preserve initialization before parenting,
  raw unsigned native strata checks, graph/level/scale bounds, dirty-batch release
  before rebuilding, callback guards and root-before-renderer/font/device teardown.
  Original m_destroyed expires attribute permissions; deferred object deletion is
  separate and retires queued descendants. Current C++ heap ownership is not the
  original pooled Lua/XML factory: adapt allocation before selecting those callers.
  The prior fixture failure and UBSan invalid enum load remain recorded without
  disabling detectors. Display events are injected, not actual SDL delivery.
  Pooled factories, region Lua/XML/language, animation/input/texture/backdrop/scroll
  consumers, complete signed ordered GlueXML plus post-creation digest gate and
  actual login remain open, then transport/gameplay. No new physical-input, Windows
  runtime, macOS graphics or server acceptance. M3 remains open; preserve original
  signatures, server revision discrepancies and interactive user sign-in.

- M3 SDL display-size [PR #51](https://github.com/RovxBot/wowdecomp/pull/51) is
  qualified at `b173d38012681e61780c6d60f6dc069782220304`, merged as
  `e36987aa8e58502708e29696a535e4d841a0dde9`. Read [window contracts](docs/m3/FRAME_WINDOW.md)
  and [validation](docs/m3/frame-window-validation.json). All thirteen implementation
  workflows passed; both Linux GL configurations passed 35 CTest checks and full
  signature-enabled Windows x64 compiled at matching-tree checkout
  `ccdb724540ca225850355886b7c69b19ff564767`. Three archive digests and all 51 packaged
  binary hashes/types verified. Exact-source RX 7900 XTX / Mesa 26.2.5 / SDL X11
  release/strict ASan/UBSan/leak packages passed eighteen authored/original-data
  window/hierarchy and prior-resource executions, with three cycles each, empty
  stderr/zero workers; four resource fixtures also passed three cycles per mode.
  Actual programmatic SDL size events refresh Gx/coordinates, invalidate Gxu/fonts/
  Frame regions, deliver the original size callback and finish pending layout before
  batching. Preserve owner/current-context and traversal guards, unhandled-event
  caller ownership, logical/drawable separation and deferred deletion. Initialize
  original coordinate math before fonts/layout as CGlueMgr::Resume does; literal
  defaults differ from recomputed 4:3 by one float ULP. The initial capped-font and
  later scale-restoration fixture failures remain recorded with assertions and
  detectors unchanged. Captures match release/sanitizer; the corrected startup
  baseline moves historical hierarchy text up one pixel, separate from PR #49's
  original capture identity. No new physical-input, independent high-DPI, Windows
  runtime, macOS graphics or server acceptance. Pooled Lua/XML/language/template
  factories, input/focus/IME/texture/animation/model consumers, complete signed
  ordered GlueXML and full post-creation digest gate, original login and transport/
  gameplay remain open. M3 is incomplete; preserve signatures, server revision
  discrepancies and interactive user sign-in.
