# M3 Linux game integration — 2026-10-24

M3 is in progress. The first work package introduces a Linux native ABI host
boundary under Gx for SDL3-owned desktop OpenGL contexts. The full native client
gate remains in place. No original login UI, networking, authentication,
character selection or world entry has been established.

Implemented in [PR #13](https://github.com/RovxBot/wowdecomp/pull/13), selected head
`2afd390c8fb9f1d7b478c915b5b9d736bcc234d9`, merged as
`d1dd70aedeae7f09468692503590c4fdbc9423ff`. The
[durable context evidence](context-validation.json) preserves all hosted manifests,
artifact/binary hashes, actual GPU reports and the separate Windows regression
merge-checkout identity `deebdb8d58b80bffefbd8eb224d0dcdc39a87fb8`.

| Evidence level | Result |
| --- | --- |
| Hosted compilation/fixtures | All eight PR workflows passed; GL release and ASan/UBSan each passed 14 CTest checks on X11/Xvfb with Mesa software rendering |
| Actual GPU rendering | Exact-head release probe on RX 7900 XTX / Mesa 26.2.4 / Xwayland authenticated 28,948 signaturefile records and displayed the original glue texture; captured frame inspected |
| Actual GPU sanitizers | Three original-data context/asset cycles, ASan/UBSan/leak detection enabled, exit 0, empty stderr, zero native workers |
| Windows regression | Full x64 client compiled with signatures enabled; downloaded binary hashes/types verified; no new Windows gameplay observation |
| Original login/gameplay | Not implemented or observed; M3 remains open |

The corrected absolute-path operator launch produced a matching exact-head
synthetic report: exit 0, empty stderr and zero workers. On 2026-10-18 the operator
confirmed resize, minimize/restore, focus changes, F11 fullscreen and window close
all worked as expected. The boxes scaled and changed aspect ratio with the window;
the graphic never went blank or crashed during the reported operations. This is
operator acceptance of the synthetic probe's window controls, separately from
automatic GPU checks. The report has no per-event telemetry or new operator
capture; it does not establish original login UI or gameplay acceptance.

The separate [12340 upstream review](../upstream/2026-10-18.md) integrates the
50-commit maintenance batch in [PR #15](https://github.com/RovxBot/wowdecomp/pull/15),
with passing hosted compilation/fixtures and focused original-PE source checks.
It preserves the native context implementation and its historical runtime
identities. Its Windows console/guild-bank/vehicle/Lua observations remain open;
it does not advance the original native login/network/world acceptance below.

The initial target was `WowNativeGl` and the executable was `NativeGlCheck`.
The later [legacy UI adapter](LEGACY_DEVICE.md) adds the allowlisted common
`CGxDevice`/global Gx/resource sources and `LegacyGxCheck`; WGL, D3D, game and
optional vendor SDK objects remain excluded. The existing Windows
client and M2 software shell keep their existing source selection. SDL remains
pinned to 3.2.28 with its existing archive hash and notices.

The Os window owner requests desktop OpenGL 3.3 compatibility, RGBA8, depth24,
stencil8 and double buffering before window creation. The Gx boundary verifies
the returned attributes, ARB vertex/fragment programs, multitexture, VBO, S3TC
and EXT FBO support and their required procedures. Missing requirements fail
visibly. Procedures and GL objects expire before their owning context/window;
all SDL/context/render operations stay on the process main thread. A native
worker loads optional original assets, then delivers one owner-thread callback.

The synthetic probe uploads ARB assembly programs, draws a textured quad into
an FBO, checks known pixels for texture coordinates, depth, blending, quad boundaries
and compressed DXT1 upload,
then presents into the window's pixel-sized drawable. It checks off-thread
presentation rejection and repeated context/window teardown. A dummy driver
negative fixture must report unavailable OpenGL and then recover a software
window after constructor failure. Hosted X11/Mesa software
rendering establishes fixture behavior, separately from a real GPU/desktop.
This is not a complete shader/material/model/terrain parity test.

Optional original data mode authenticates the original 12340 signaturefile and
loads `Interface\Glues\LoadingBar\Loading-BarFill.blp` using the existing MPQ,
patch/locale and registered-MD5 policy. It displays that texture through ARB
programs. A loading-bar asset is not the original GlueXML login UI.

All compilation ran in [M3 OpenGL Actions](../../.github/workflows/m3-gl.yml).
[Push run 39083361222](https://github.com/RovxBot/wowdecomp/actions/runs/39083361222)
and [PR run 39083364041](https://github.com/RovxBot/wowdecomp/actions/runs/39083364041)
passed both Linux configurations. Full Windows regression
[39083364138](https://github.com/RovxBot/wowdecomp/actions/runs/39083364138) passed.
Local source/launcher checks discovered 22 tests: 21 passed, one CMake selection
check skipped because local CMake is unavailable; that check runs in Actions.
Local work executed downloaded binaries and Python source/document checks only.
No local client or dependency compilation is permitted.

After downloading a passing `m3-linux-gl-*` artifact, extract it outside the
repository and both installations. The launcher checks the binary hash and
uses X11 with a matching child EGL platform:

```sh
python3 Tools/Porting/run_native_gl.py \
  --artifact /tmp/wowdecomp-m3-2afd-release \
  --data /mnt/games/3.3.5aClean/Data \
  --settings /tmp/wowdecomp-m3-settings --cache /tmp/wowdecomp-m3-cache \
  --frames 3600 --timeout 120 \
  --snapshot /tmp/wowdecomp-m3.ppm --output /tmp/wowdecomp-m3.json
```

Run from the repository or use the absolute wrapper path
`/home/Rov/Documents/Git/wowdecomp/Tools/Porting/run_native_gl.py` from another
directory. The prepared artifact path above is local runtime state; after artifact
expiration, rebuild through Actions and verify the new manifest/source identity.

Omit data/settings/cache for the synthetic quad. Use the sanitizer artifact with
`--sanitizers`; keep all detectors enabled and retain external-library failures.
Escape/window close exits; F11 toggles fullscreen. Operator checks: resize,
minimize/restore, focus changes and fullscreen must keep the image stable and
the drawable dimensions correct. Record actual GPU/driver, X11 versus Xwayland,
artifact/commit, captures and errors. This does not request credentials or a
server connection. M2 input/settings acceptance remains separate.

The observed desktop is Xwayland under a Wayland session, using SDL's X11 driver
and child `EGL_PLATFORM=x11`. This is not a separate native Xorg or M3 Wayland
driver qualification. GPU sanitizer runs used no `LSAN_OPTIONS`, `LD_PRELOAD` or
`LD_AUDIT`; the existing M2 Wayland Fontconfig qualification is unchanged. Captures
remain external; only their hash/dimensions and inspection results are recorded.

The [native shader work package](SHADERS.md) is merged in
[PR #17](https://github.com/RovxBot/wowdecomp/pull/17), selected implementation
`543243c676f16b1a2c32af38d8b0910c73eaadbd`, merge
`7c5f5ec4feb59760a25aec07d30dbaa433826da5`. The [shader record](shader-validation.json)
retains all eight passing PR workflows, separate Windows merge-checkout identity,
15 CTest passes in each Linux GL configuration, and exact-source original-data
GPU release and three clean ASan/UBSan/leak-enabled cycles. Both original vertex
UI permutations uploaded; permutation zero and the original fragment UI shader
passed five texture/depth/blend/FBO pixels and displayed the original loading-bar
texture. Both UI shader files have no registered MD5 record; the original-key
signaturefile and existing selective MD5 policy remain enabled. This adapts a
native shader resource service, not the complete legacy device/native ABI.
Original fonts/GlueXML/login, networking and gameplay remain open. On 2026-10-18
the operator confirmed that resize, minimize/restore, focus changes, F11 and window
close all passed on the exact `543243c` original-data shader probe, with no crash
or corruption. The saved release report matches its hosted source/binary identity
and records exit 0, no timeout, empty stderr and zero workers. These physical
observations are recorded separately from automatic shader/FBO checks. The report
has no per-event telemetry or additional capture; prior context-probe acceptance
above remains attached to `2afd390`. Neither acceptance establishes original login
UI or gameplay.

The prepared final artifact is `/tmp/wowdecomp-m3-shaders-release`; use it in the
[shader operator command](SHADERS.md#validation-and-operator-use). The command
checks the binary hash, uses the unchanged read-only clean reference and external
settings/cache. No new server connection or credentials are needed.

A [native render-target owner](RENDER_TARGETS.md) is qualified in
[PR #20](https://github.com/RovxBot/wowdecomp/pull/20), selected implementation
`2eb6dd9ee61c15aa5b788a6f7856cdb2019e9b4c`; merged as
`4d74dd0c5330f92c3fc282329fc071c61b23f62a`. It replaces the
shader probe's inline FBO with retained texture/depth ownership, attachment
transitions, cube/depth-only targets, feedback rejection and drawable restoration.
The [durable validation](render-target-validation.json) records all eight passing
implementation PR workflows, full Windows x64 compilation at separate merge
checkout `30a0acafc0af371f9bb1f77ee6360585ece00cbf`, and 16 CTest passes per Linux
GL configuration. Exact-source RX 7900 XTX / Mesa 26.2.5 / SDL X11 release and
ASan/UBSan/leak runs passed three target cycles each, plus one release and three
sanitizer original-data shader cycles. Both executables had empty stderr and all
probe cycles closed with zero workers. The release capture was inspected.
This is automatic GPU/fixture evidence, with no new physical-input acceptance.
Prior operator acceptance retains its own implementation identity above.
Full device/state/projection, original fonts/login UI and gameplay remain open.

A separate [live server observation](SERVER_IDENTITY.md) now records image IDs,
running binary identity, mounted Warden and listener metadata externally. The
embedded core revision differs from the package tag and has a `+` suffix; neither
the desired tag nor local checkout identifies an exact clean compiled source tree.
No native authentication, advertised realm-list endpoint or Warden session was
qualified. Preserve this later snapshot separately from historical M0 observations.

The next [transform work package](TRANSFORMS.md) adapts existing scalar Gx/Tempest
matrices, normalized viewport/scissor, offscreen projection/culling and original
UI shader constants to the context/target owner. It is stacked on PR #20 and
is qualified at `a1c43f820448cbb0b0c0eb2fda2ab59216687415` in
[PR #21](https://github.com/RovxBot/wowdecomp/pull/21), merged as
`37bc74b492ce3f547d8b222676a7cc547e7727ab`. The
[transform evidence](transform-validation.json) records all eight implementation
PR workflows, 17 CTest passes per Linux GL configuration, separate Windows merge
checkout `b18ed2af2622a8dd15e67a5863a9997b1f143f13`, and exact-source RX 7900 XTX
release/sanitizer target/transform/original-data shader cycles. These include
scaled/translated original UI shader pixels, empty stderr and zero probe workers.
Fresh compatibility matrix-mode initialization and validated unsigned stack
indices fix two recorded initial failures; nonidentity texture-matrix delivery
fails visibly until integrated. This remains a native boundary, not the full
legacy CGxDevice/CGxTex/state-cache or original login UI. No new physical-input
acceptance or server session is recorded. M3 remains open.

The next [buffer/vertex work package](BUFFERS.md), following merged PR #21,
is qualified at `42859739d5359cffc0b55f33ebfed789ff152dcd` in
[PR #22](https://github.com/RovxBot/wowdecomp/pull/22), merged as
`fd0ae32545f6da42b4fa58572eb26953131324c7`. It connects
retained vertex/index resources and checked submission to the existing probe,
replacing immediate-mode quads with streamed PCT triangles and uint16 indices.
The [buffer evidence](buffer-validation.json) records all eight successful PR
workflows, 18 CTest passes per Linux GL configuration, separate Windows compilation
checkout `827d3cdd0876014389239e3ae40eea8d053e6a5e` and verified artifact identities.
Exact-source RX 7900 XTX release/sanitizer buffer, transform and target checks each
passed three cycles; the original-data shader probe passed one/three cycles,
with empty stderr and zero workers. The initial synthetic conventional/generic
attribute mismatch and its correction remain recorded. This does not export
legacy CGxPool/CGxBuf objects or complete global device/state/texture/async
integration. No new physical-input, original fonts/login or server acceptance
is implied. M3 remains open.

The [sampled-texture work](TEXTURES.md) now extends the same owner to bounded
mip/face/rectangle uploads, packed font-atlas formats and DXT resources, and
removes raw texture allocation/upload from the probe. It is qualified at
`8e4fcde51d7962411d27c462655ad677f95aa23c` in
[PR #23](https://github.com/RovxBot/wowdecomp/pull/23), merged as
`365016e226547f7f3867f419988ccdf6b3182f25`. The
[texture record](texture-validation.json) retains all eight passing PR workflows,
19 CTest passes per Linux GL configuration, separate Windows compilation identity,
verified artifacts and exact-source RX 7900 XTX release/sanitizer resource and
original-data shader cycles. These passed with empty stderr and zero workers.
The atlas fixture is synthetic; legacy texture callbacks/cache and original
font/login rendering remain open. The user will sign in interactively when
native authentication becomes available.

The [original font work](FONTS.md) adds an owner-thread memory face using the
bundled FreeType sources and a bounded ARGB4444/PCT text preview through original
UI shaders. It is qualified at `16c111a5f36864530a2109ff7228a78f68fb17fd` in
[PR #24](https://github.com/RovxBot/wowdecomp/pull/24), with
[durable evidence](font-validation.json) for nine passing PR workflows, 20 CTest
checks per Linux GL configuration, full Windows x64 compilation at separate
merge checkout `528822b509acfb57869cb65591edea824ddd09e9`, and verified artifacts.
Exact-source RX 7900 XTX release/strict sanitizer original-data font/shader cycles
passed one/three times with empty stderr and zero workers. Resource fixtures
passed three cycles each in both modes; the release text capture was inspected.
The prior FreeType failures and native-only corrections remain recorded, including
font-program execution despite unhinted glyph flags. This does not complete
legacy HFACE/Gxu cache/layout, [global Gx dispatch](LEGACY_GX.md) or GlueXML/login.

The [packet prerequisite package](PACKETS.md) prepares original `CDataStore`,
ARC4 and SHA1/HMAC code for native transport integration, with explicit unaligned
wire accesses and bounded native ownership. It is qualified at
`6d1a9c209410d42f4351e71645c44edbe891cfb2` in
[PR #25](https://github.com/RovxBot/wowdecomp/pull/25). Its
[record](packet-validation.json) retains nine passing PR workflows, five CTest
checks/three packet cycles in each of four native configurations, verified ARM64
macOS identity and separate full Windows compilation/artifact identity.
No sockets or server session are implemented by it. Source inspection
also identifies `WowConnectionNet`'s WSA event/worker dispatcher, in addition to
the Os IOCP transport, as required networking work. The original login/gameplay
gate and the external server revision discrepancy remain unchanged.

The original-font PR #24 is merged as
`5090a23e6fc46f572df301ac8f1a9a13b29c409b`. The subsequent
[Storm string/cache prerequisite](LEGACY_STRINGS.md) in
[PR #26](https://github.com/RovxBot/wowdecomp/pull/26) selects original string,
option and ordered-format sources in the native core and exercises actual
`TSHashTable` name ownership. Follow-up source review corrects variadic argument
preflight, misplaced format lengths, caller-owned argument-list reuse and the
original `I64` spelling in ordinary GUID/achievement formats. It is qualified at
`2d946465e52d7ccfb43722d844d76a4fa4cd038b`; the [record](string-validation.json)
retains all nine passing PR workflows, four native CPU configurations, 20 CTest
checks per Linux GL configuration, the separate Windows merge-checkout identity
and seven verified artifacts. Exact-source original-data GPU release/strict
sanitizer regressions passed one/three cycles with empty stderr and zero workers;
all four resource fixtures passed three cycles in both modes. These identities
remain separate from the historical font and packet evidence above. This
prerequisite does not connect legacy device dispatch or render the original login UI.

The [legacy UI dispatch adapter](LEGACY_DEVICE.md) in
[PR #27](https://github.com/RovxBot/wowdecomp/pull/27), originally stacked on PR #26,
now connects actual common Gx state/cache/resource objects to the native owners.
It is qualified at `6ef31f4a94cef0d1b1947f46e4e35d716568b520`. The
[record](legacy-device-validation.json) retains nine passing implementation PR
workflows, 21 CTest checks per Linux GL configuration, separate full Windows x64
compilation, three verified archives and 31 packaged binary hashes/types.
Exact-source RX 7900 XTX / Mesa 26.2.5 / SDL X11 release and strict sanitizer
checks each passed three original-data legacy callback/PCT/font/UI-shader cycles,
with empty stderr and zero probe workers. Existing preview/resource regressions
also passed; the independent preview capture was inspected. The initial source/
fixture/RTTI link failures remain recorded without disabling sanitizers. This bounded 2D/UI step does not
export original Gxu HFACE/layout/cache or render the original login UI. Its
single-mip/format/state limits remain explicit instead of advertising the full
underlying native resource surface as a completed legacy adapter.

PRs #26 and #27 are merged as `c3ef9fd989d15beb4af2695ea0cc08865e23bd19`
and `6dae38e729a44c8df2424a0f27d6b892fcac7b0f`. Their historical implementation
and build identities above are unchanged. The following
[original Gxu integration](LEGACY_GXU.md) in
[PR #28](https://github.com/RovxBot/wowdecomp/pull/28) is qualified at
`ade47622c9023fee38fce1b6346be2f8be51fce4`, merged as
`d07e289755d303accd0f97d7d49bcfe21b84d538`. Its [record](gxu-validation.json)
retains all nine passing implementation workflows, 22 CTest checks per Linux GL
configuration, full Windows x64 compilation at matching-tree PR merge checkout
`ca7e229e536c5434c3e9c6d26f6c8bf03494eb6a`, three verified archives and all
33 packaged binary hashes/types. Original HFACE, CGxFont/CGxString,
HTEXTFONT/HTEXTBLOCK, layout/markup/kerning, glyph/row/string caches and batched
rendering now consume actual native-backed legacy objects. File-backed Services
textures/async callers and original Frame/Lua/GlueXML are not included.

Exact-source RX 7900 XTX / Mesa 26.2.5 / SDL X11 release and strict
ASan/UBSan/leak checks each passed three original-data Gxu/Services cycles and
three authored cache-pressure cycles, with empty stderr and zero workers.
The inspected 640x480 capture shows original red/white Friz Quadrata glyphs.
Friz has no glyphs in the tested stress range, so authored 2,048-glyph LRU
eviction/regeneration is separate from original-data rendering. Prior legacy
Gx, independent font preview and all four resource regressions passed too.
Initial link, drawable and pixel-center failures remain recorded. Preserve
original OpenGL pixel/texel centers and scalar epsilon, main-thread mutation,
retained face bytes and consumers-before-font/device/context teardown. No new
physical-input, macOS graphics, Windows runtime or server acceptance is implied.

The [product Lua runtime prerequisite](LUA.md) in
[PR #29](https://github.com/RovxBot/wowdecomp/pull/29) is qualified at
`33ef88fdd742b6b2a4484f3c86c013642f977837`, merged as
`f1726d9628bd15d09f8acee4442fff5976dac81f`. Its four native configurations are
Linux release/strict ASan/UBSan/leak, actual ARM64 macOS and Windows x64.
Each passed six CTest checks and three Lua cycles with profiling disabled and
three enabled. The original pool, modified VM, five libraries, taint/accounting
and profiling APIs are selected; executable callbacks remain restricted to the
main image. LP64 parsing/formatting and x86 bit shifts retain Windows widths.
Original source-only loading remains enforced. At that prerequisite checkpoint,
full FrameScript/Frame/XML, file/event/secure-handler ownership and original login
rendering remained open.
The [record](lua-validation.json) retains all ten passing implementation PR
workflows, five verified archives/19 binary hashes and the separate matching-tree
Windows checkout `3c48f75f4e9bbe94a4e5c09c612fb9c88652acb3`. Downloaded Linux
release/strict sanitizer executions passed six cycles each, with expected
allocation-overflow diagnostics and no sanitizer finding. No new GPU, physical
input, Windows runtime, macOS desktop or server acceptance is implied.

The [original FrameScript integration](FRAMESCRIPT.md) in
[PR #31](https://github.com/RovxBot/wowdecomp/pull/31) is qualified at
`d49efe426d6fc590dc570b05d2bddb3bf4a36b38`, merged as `872ca9d962e73095a04ec7e5d816f27c59df35cb`.
The [record](framescript-validation.json) retains all eleven passing implementation
PR workflows, 12 CTest checks per native configuration, four verified native
archives/40 executable hashes and the separate full Windows compilation archive
and three PE x64 identities. The matching-tree hosted checkout is
`95ef62bd82e4accbd1655f6e89fe50933e706369`. Original execution, event listeners,
compatibility helpers, secure calls/hooks, profiling, saved-variable closure taint
and serializer now consume the qualified Lua runtime under an owner-thread VM
lifecycle. Nested execution and live FrameScript objects prevent VM reset.

Downloaded release and strict ASan/UBSan/leak binaries each passed six authored
cycles and six unchanged original `GlobalStrings.lua` cycles with empty stderr
and zero workers. Original-key signaturefile authentication and registered-MD5
policy remain enabled; this Lua member has no registered MD5 entry. TOC/interface
signature traversal and actual Frame/XML/GlueXML/login rendering remain open.
The initial fixture's mistaken table-value taint expectation is retained as failed
evidence; the original contract marks newly created closures instead. No new GPU,
physical-input, Windows runtime, macOS desktop or server acceptance is implied.

The [original XML/signature integration](XML.md) in
[PR #33](https://github.com/RovxBot/wowdecomp/pull/33) is qualified at
`c79cd56a6854ab15e9cf89de47c4c2a3a7663b04`, merged as `c4ee06549c7f5cc5b6093a7d3189c8c6d1dac6bf`.
All twelve implementation PR workflows passed, including full Windows x64
compilation at matching-tree checkout `f7c2de912a17e55ef9196379b50c097c00dfbc8d`.
Four native configurations passed 14 CTest checks/three authored XML cycles each;
five archives and all 47 packaged executable hashes/types were verified.
Downloaded Linux release/strict ASan/UBSan/leak binaries each passed three
authored and three original-data cycles with empty stderr/zero workers.
The original InterfaceKey authenticated both interface sidecars: GlueXML MD5
`b44247235136edcfbbc314797eb2c705` (60 visits) and FrameXML plus Bindings MD5
`51b264400410d3409c4e79f6f4257711` (267 visits). Original AccountLogin XML parsed;
frames/Lua/login were not executed or rendered. The [record](xml-validation.json)
retains two compiler failures and the failed restricted-executor LeakSanitizer
ptrace attempt; the same binary passed outside tracing with all detectors active.
Preserve XML-before-VM shutdown, borrowed-node lifetime, native pointer alignment,
raw manifest order and the original post-creation digest comparison. No new GPU,
physical-input, Windows runtime, macOS desktop or server acceptance is implied.
M3 remains open.

The [file-backed image Services](IMAGES.md) in
[PR #35](https://github.com/RovxBot/wowdecomp/pull/35) are qualified at
`db24f90dc2d845d5a5944df0cedb46fd116d13ec`, merged as
`b1479843dd7eac430514fc1103a4e9c1e6e39301`. The [record](image-validation.json)
retains all twelve passing implementation PR workflows, 23 CTest checks per Linux
GL configuration, full Windows compilation at matching-tree checkout
`b3df743c8c009d1076d9ec4acfbe3cbd6ba50e86`, three verified archives and all 35
packaged executable hashes/types. Reads/decode use the retained worker; actual
`CTexture` handles/cache/Gx callbacks mutate on the owner thread. Last-handle close
cancels retained reads without publishing a native ticket as `CAsyncObject`.

Exact-source RX 7900 XTX / Mesa 26.2.5 / SDL X11 release and strict
ASan/UBSan/leak binaries each passed three authored and three original-data image
cycles. Original 64x64 loading-bar Services/PCT/UI-shader capture hashes match;
the release capture was inspected. Gxu/legacy Gx, preview and all four resource
regressions passed, with empty stderr and zero probe workers. Initial authored
minimum-dimension and offscreen/BGR expectation failures remain recorded. Original
signatures remain enabled. This single-mip, explicit nearest/linear BLP2 surface
does not yet accept Frame's `0x17` flags, wait mode 2, atlas/TGA/streaming callers
or render original Frame/GlueXML/login. No new physical-input, Windows runtime,
macOS graphics or server acceptance is implied. M3 remains open.

The subsequent [authored mip/readiness extension](IMAGE_MIPS.md) in
[PR #37](https://github.com/RovxBot/wowdecomp/pull/37) is qualified at
`41c0a78e7f25bf6a040a59560c5f33502297d6f9`, merged as
`0d90dcdd741e43418016aaeeb15a66c23be2f1f8`. Its
[record](image-mip-validation.json) retains all seven passing triggered PR
workflows, 23 CTest checks per Linux GL configuration and full Windows x64
compilation with signatures enabled at matching-tree checkout
`6c2521c2a897f9e4d192404b118139f784b7e22b`. Three archive digests and all 35
packaged executable hashes/types were verified. Exact-source RX 7900 XTX release
and strict ASan/UBSan/leak execution passed three authored and three original-data
image cycles each, including minified original Elwynn authored mip chains through
Services/Gx/UI shaders. Gxu/Gx/preview/resource regressions passed; all eighteen
executions exited 0 with empty stderr and zero probe workers. Loading-bar capture
hashes match the prior qualification. No new terrain/world capture, physical-input,
Windows runtime, macOS graphics or server acceptance is implied.

The service now accepts explicit authored mip filters, local wait 2 and UI priority
0x10, retaining exactly-once Unlock and a shared pixel budget. Frame 0x17 still
requires disk/atlas adaptation; TGA/default-policy/streaming and actual original
Frame/GlueXML/login remain open. The PR #35 identities above remain historical.

The [original Frame layout prerequisite](LAYOUT.md) in
[PR #39](https://github.com/RovxBot/wowdecomp/pull/39) is qualified at
`4aa176506079a35e81c3bf9d7108e7bff1d57df2`, merged as
`4cf5e0e536dca1bb96f06d8a8500e7ddd293c754`. Its [record](layout-validation.json)
retains thirteen passing implementation workflows, 15 CTest checks/three authored
layout cycles in each of four native configurations, full Windows x64 compilation
at matching-tree checkout `8e4de17077fd39972148f7b0aa1101a28e711dd6`,
five verified archives and all 51 executable hashes/types. Original layout/anchor,
resize-list and XML coordinate consumers now have native ownership, initialized
flags, bounded graphs and transitive rectangle invalidation. Downloaded Linux
release/strict sanitizer layout and original-data XML/signature regressions each
passed three cycles with empty stderr. Original Frame factories/login rendering
are still open; this adds no GPU, input, Windows runtime, macOS desktop or server
acceptance. Original signatures and interactive sign-in remain intact.

The [original Frame font/name factories](FRAME_FONTS.md) in
[PR #41](https://github.com/RovxBot/wowdecomp/pull/41) are qualified at
`c87df98ab5b8f05cab034941dece92a78f81d726`, merged as
`829904d849546a224201a885fdb4e1c55ce578e5`. The [record](frame-font-validation.json)
retains thirteen passing implementation workflows, 31 CTest checks per Linux GL
configuration and full Windows x64 compilation at matching-tree checkout
`ba54f09734f163123155e378048cadf3bcfde61b`. Three archives and all 45 packaged
binary hashes/types were verified. Exact-source RX 7900 XTX / Mesa 26.2.5 / SDL X11
release and strict ASan/UBSan/leak checks passed three authored movement/font,
three original GlueFonts and three original-data Gxu cycles each, with empty
stderr and zero workers. All 18 original GlueFonts definitions were constructed;
a selected small outlined font rendered at its original logical size, with exact
repeat pixels and matching release/sanitizer captures.

Native font/name factories now use the original Lua/XML/Gxu owners and canonical
live Lua tables. Native FreeType movement adapters preserve the original index
conversion and Windows paths; the strict prior failure and authored reproducer
remain in the record. Sample text/red tint are authored fixture inputs, separate
from original font bytes/definitions. Actual regions/font strings/root batching,
SDL size invalidation, disk/atlas images and full GlueXML/login remain open.
There is no new physical-input, Windows runtime, macOS graphics or server session
acceptance. M3 remains incomplete; signatures and interactive sign-in stay intact.

The [native image lookup/atlas package](IMAGE_SOURCES.md) in
[PR #43](https://github.com/RovxBot/wowdecomp/pull/43) is qualified at
`e308d2e7eedc1e8f8cba7cfc8cec885e8fe5bfa6`, merged as
`dbb4a00d8c85b74ad11f34a8a5f68aa91692aa1e`. The
[record](image-source-validation.json) retains thirteen passing workflows,
31 CTest checks per Linux GL configuration and full Windows x64 compilation at
matching-tree checkout `e520213854a27c5fb2c694e3effbb0b9e3443821`, with signatures
enabled. Three archives/all 45 packaged binary hashes/types were verified.
Downloaded release/strict sanitizer binaries each passed three authored image,
three original-data image and three original GlueFonts cycles with empty stderr
and zero workers. Loading-bar captures match prior qualification; the original
BLP remains standalone because it has no atlas flag. Marked shared-atlas rendering
and loose-file overrides are authored evidence. The first fixture's queued-read
count failure remains recorded.

Native Services now interpret disk/atlas `0x17` eligibility, mounted read-only
loose-root lookup, original 8 by 8 atlas coordinates/slot lifetime and default
filtering. The bounded atlas uses single-level clamped nearest/linear sampling;
repeat/mip or other ineligible images retain standalone storage. TGA/alternate
lookup and streaming remain unavailable. Actual Frame regions/root/image modes,
full GlueXML/login and gameplay remain open. No new physical-input, Windows
runtime, macOS graphics or server acceptance is recorded. M3 remains incomplete.

The [original Frame render batcher](FRAME_RENDER.md) in
[PR #45](https://github.com/RovxBot/wowdecomp/pull/45) is qualified at
`d09ad319696fa7a05767bcc2af5f810af4378b3d`, merged as
`d4b213b01e9a9d50a538c50f3247614930058517`. Its [record](frame-render-validation.json)
retains thirteen passing workflows, 32 CTest checks per Linux GL configuration,
full signature-enabled Windows x64 compilation at matching-tree checkout
`08dbdbc46ee4b5b0100aeaa427facc731d81332c`, three archive digests and all 47 packaged
binary hashes/types. Release and strict sanitizer binaries each passed three
authored batch, three original-data batch, three GlueFonts, three image and three
Gxu cycles on RX 7900 XTX / Mesa 26.2.5 / SDL X11, with empty stderr and zero workers.
All four resource fixtures passed three cycles per mode; captures match.

Actual `CRenderBatch`/`CSimpleRender` counting and draw bodies now retain quad
snapshots and texture/shader/font/block handles, use full-width sorting, preserve
merge splits and atlas UVs, guard callbacks and recover partial staging writes.
Three earlier fixture failures remain recorded. The original-resource composition
is authored, with original fonts/shaders/BLP; it is not original Frame/GlueXML/login.
Actual Frame-object queues, parent/animation/layer/root and deferred lifetimes, SDL
size invalidation and texture modes remain open. No new physical-input, Windows
runtime, macOS graphics or server acceptance; M3 remains incomplete.

The [original unparented font-string regions](FRAME_REGIONS.md) in
[PR #47](https://github.com/RovxBot/wowdecomp/pull/47) are qualified at
`01ac7deb547aa3216a92d78f93b6eb07e59eb9a0`, merged as
`259518fd512a7c9cf4f4dcd8a4e0b0998058f282`. The
[record](frame-regions-validation.json) retains thirteen passing workflows,
33 checks per Linux GL configuration, full signature-enabled Windows compilation
at matching-tree checkout `c71a91a609e2a301415cd46e81cfff84b826613e`, three archive
digests and 49 packaged executable hashes/types. Downloaded release and strict
sanitizer binaries each passed three authored/original region cycles and three
each of Frame batch/GlueFonts/image/Gxu regressions, plus all four resource
fixtures, with empty stderr and zero workers. Original region captures match.

Actual `CScriptRegion`/`CSimpleRegion`/`CSimpleFontString` now consume qualified
font attributes/layout/Services and retained batching through direct unparented
C++ calls. Font replacement preserves block-before-font cleanup; queued resources
survive consumer destruction. Callback/layout guards, native field initialization,
logical-pixel rounding and script-factory rejection preserve ownership. Prior
pending-layout fixture and RTTI linkage failures remain recorded with detectors
enabled. Original resources and eighteen GlueFonts definitions are used in authored
composition; this is not original login. Root/parent/layer, animation/Lua/XML,
texture factories and real SDL size delivery remain open. No new physical-input,
Windows runtime, macOS graphics or server acceptance; M3 is incomplete.

The [original Frame parent/layer/root integration](FRAME_HIERARCHY.md) in
[PR #49](https://github.com/RovxBot/wowdecomp/pull/49) is qualified at
`ac717d347e2226e06a4f36e0a2bd485f396f6ef2`, merged as
`9b01385d6a1bdb11a78905dd9c302d559cbc142d`. The
[record](frame-hierarchy-validation.json) retains thirteen passing PR workflows,
34 checks per Linux GL configuration, full signature-enabled Windows x64
compilation at matching-tree checkout `960ebca2d4bb7a50a7cd8ddd53f3e44a89f02274`,
three verified archives and all 51 packaged binary hashes/types. Downloaded
release/strict sanitizer binaries each passed three authored and three original-data
hierarchy cycles, plus region/batch/GlueFonts/image/Gxu and all four resource
regressions, with empty stderr and zero workers. Captures match; original-resource
text/layout is authored, not login. The retained fixture failure and UBSan enum-load
finding were corrected with all detectors enabled.

Actual parent/child, region/layer/strata lists, inherited visibility/alpha/scale and
root batches now use original Frame consumers through a direct C++ heap-owned
surface. Preserve `m_isShown` initialization before parenting, unsigned native
strata preflight, bounded parent/level/scale graphs, dirty-batch retirement before
rebuilding and update/collection/resize/draw guards. Original `m_destroyed` expires
attribute permission; the separate deferred object queue coalesces requests and
retires recursively deleted descendants. Close root consumers before renderer/
font factories/Gxu/layout/XML/VM/device/context/queue. Pooled Lua/XML factories,
animation/input/texture consumers and actual SDL size delivery remain open. The
complete signed ordered GlueXML plus post-creation digest gate and original login
remain open, followed by transport/gameplay. No new physical-input, Windows runtime,
macOS graphics or server acceptance. M3 stays incomplete.

The [SDL display-size integration](FRAME_WINDOW.md) in
[PR #51](https://github.com/RovxBot/wowdecomp/pull/51) is qualified at
`b173d38012681e61780c6d60f6dc069782220304`, merged as
`e36987aa8e58502708e29696a535e4d841a0dde9`. Its
[record](frame-window-validation.json) retains thirteen passing workflows,
35 checks per Linux GL configuration and full signature-enabled Windows x64
compilation at matching-tree checkout `ccdb724540ca225850355886b7c69b19ff564767`.
Three archives and all 51 packaged binary hashes/types verified. Downloaded release
and strict ASan/UBSan/leak packages passed eighteen authored/original-data window,
hierarchy and prior-resource executions, with three owner cycles each, empty stderr
and zero workers; all four resource fixtures passed three cycles per mode.

Actual programmatic SDL size events now refresh Gx dimensions and coordinate
aspect, invalidate original Gxu fonts and Frame regions, deliver the original
size callback and finish pending layout before batching. Preserve the current
context/owner and traversal guards, unhandled-event caller ownership, logical
versus drawable dimensions and deferred destruction. Initialize original coordinate
math before fonts/layout, as `CGlueMgr::Resume` does. The prior capped-font and
startup-order fixture failures remain recorded with unchanged assertions/detectors.
Release/sanitizer captures match; the corrected startup baseline moves historical
hierarchy text up one pixel, retained separately from PR #49's capture identity.
This is authored layout with original resources, not login. Physical keyboard/mouse/
focus/IME, independent high-DPI/platform coverage, pooled Lua/XML factories,
texture/animation/model consumers, complete signed ordered GlueXML and its full
post-creation digest gate, login and transport/gameplay remain open. No new
physical-input, Windows runtime, macOS graphics or server acceptance. Preserve
signatures, server revision discrepancies and interactive user sign-in; M3 stays open.

Next dependencies, in order:

1. Continue from the qualified [Frame parent/root integration](FRAME_HIERARCHY.md).
   Adapt original pooled factories before selecting Frame/region Lua/XML/language
   callers and animation ownership. Preserve the qualified [SDL size path](FRAME_WINDOW.md)
   while extending keyboard/mouse/focus/IME and input/occlusion consumers.
   Preserve qualified [font/name](FRAME_FONTS.md), [layout](LAYOUT.md) and retained
   batch lifetimes; direct C++ ownership does not install those factories.
   Connect actual Frame texture factories and image-mode shaders to the qualified
   [image lookup/atlas surface](IMAGE_SOURCES.md); retain readiness and atlas
   coordinates. Extend device/state/
   mip/texture contracts as their callers require them; audit texture/texgen
   delivery and legacy async callers.
   Do not pass native file tickets as `CAsyncObject` or use inspection geometry
   as a replacement for full M2 animation/material ownership.
2. Use the qualified XML trees/interface signatures while connecting FrameScript
   to actual Frame factories and GlueXML/FrameXML execution. Retain the original
   post-creation digest comparison; follow the [next-caller audit](XML.md#next-original-callers).
   Render the actual original login UI; extend unchanged 12340 `0x10003` shader
   and full model/material coverage as required by its callers.
   The Windows OpenGL warped-scene failure remains a known comparison failure;
   obtain fixed D3D9 and OpenGL captures before claiming visual parity.
3. Replace IOCP beneath the original network interfaces, with framing, partial
   I/O, cancellation and lifetime fixtures; preserve 12340 authentication and
   packet layouts. Observe character selection and world entry on GrimGuzzler.
4. Refresh the external live server snapshot for the actual gameplay session,
   verify the realm-list advertised endpoint and record Warden outcomes. The
   current image/binary/mounted-policy observation does not establish those
   session behaviors or an exact clean server source tree. Do not change server
   or cluster policy or disable client signature checks to qualify a session.

M3 exits only after an observed native Linux world session with unchanged data.
Audio/movie limitations and unavailable platform coverage must stay explicit.

The [SDL keyboard/text/focus adapter](FRAME_KEYBOARD.md) in
[PR #53](https://github.com/RovxBot/wowdecomp/pull/53) is qualified at
`75225260de3b03a17b2f280ab75f377a9f295d42`, merged as
`74eeee2399937ed72da27f308e9d66ba370d8f23`. Its
[record](frame-keyboard-validation.json) retains thirteen passing PR
workflows, 36 checks per Linux GL configuration, signature-enabled full Windows
x64 compilation at matching-tree checkout `c2f06f3b20e3335f976682bb28fc1f709828663c`,
three verified archives and all 51 packaged binary hashes/types. Downloaded RX
7900 XTX release/strict ASan/UBSan/leak packages passed 22 authored/original-data
keyboard/window/hierarchy and prior-resource executions, three cycles each, with
empty stderr/zero workers; all four resource fixtures passed three cycles per mode.
Captures match the prior window baseline and each other.

Original root stratum/priority queues now deliver native key-down, captured
repeat/key-up, committed Unicode scalars and focus callbacks. Native storage
covers the full key enum, including Super keys; validate raw indices before
array/shift access. Preserve original modifier bits, event identities/timestamps,
PrintScreen fallback, hide/disable/destruction cleanup and traversal guards.
Native priority nodes use matching allocation/deallocation. Callback consumers
may defer deletion; finish input before root mutation/update/render/destruction.
Nonempty native Lua input handlers still reject before stack mutation until actual
pooled Lua/XML factories are selected. Authored SDL_PushEvent checks are not
physical-input or IME/language acceptance. Mouse/occlusion/drag/IME, pooled
factories, texture/animation/model consumers, full signed ordered GlueXML plus
post-creation digest gate, actual login and transport/gameplay remain open.
No new physical-input, Windows runtime, macOS graphics or server acceptance.
M3 stays incomplete; preserve signatures, server revision discrepancies and
interactive user sign-in.

The [SDL IME adapter](FRAME_IME.md) in
[PR #55](https://github.com/RovxBot/wowdecomp/pull/55) is qualified at
`77177a99bcd2ef524c42bfb9bdeba1081cd07f61`, merged as
`da722de8093aba62baa59805c11e35c7e6b857b9`. Its [record](frame-ime-validation.json)
retains thirteen passing PR workflows, 37 CTest checks per Linux GL configuration,
signature-enabled full Windows x64 compilation at matching-tree checkout
`5629a4093141fe46d9e140c4793620bac0066828`, three verified archives and all 51 packaged
executable hashes/types. Candidate-only focus cancellation also passed on the
SDL-only actual ARM64 macOS, Windows and three Linux configurations.

Downloaded RX 7900 XTX release/strict ASan/UBSan/leak packages passed 26 authored/
original-data IME, keyboard/window/hierarchy and prior-resource executions, three
cycles each, with empty stderr/zero workers; all four resource fixtures passed
three cycles per mode. Captures match the prior baseline and each other. Native
composition/candidate/result events now use original stratum/priority CHAR queues,
typed consumers and event ID with explicit UTF-8 code page 65001. Deliver immediately
after OsQueueGet, before another dequeue replaces the borrowed OsIME snapshot;
consumers must not pump/drain the queue. Raw actions/flags are bounded before
callbacks, and traversal guards permit deferred deletion while rejecting mutation,
destruction and reentry. Focus loss cancels candidate-only sessions. Native
language-mode action zero remains unsupported by SDL. This is injected input,
not physical IME/language acceptance or actual edit-box/candidate UI.

Pooled Lua/XML/language/template factories, mouse/occlusion/drag, other Frame
consumers, full signed ordered GlueXML plus post-creation digest gate, actual login
rendering and transport/gameplay remain open. No new physical-input, Windows
runtime, macOS graphics or server acceptance. M3 remains incomplete; preserve
signatures, server revision discrepancies and interactive user sign-in.

The [bounded SDL pointer adapter](FRAME_MOUSE.md) in
[PR #58](https://github.com/RovxBot/wowdecomp/pull/58) is qualified at
`205d3d5770c3ca756909f9c0de63c7a0f3a294a3`, merged as
`53c259f20cb48db3e5e6a4011074b5d0644ad447`. Its [record](frame-mouse-validation.json)
retains thirteen passing PR workflows, 38 CTest checks per Linux release/strict
ASan/UBSan/leak configuration, signature-enabled full Windows x64 compilation at
matching-tree checkout `51970b1b468c9a5f5310a56d742895fe913bf414`, three verified
archives and all 51 packaged binary hashes/types/notices. Downloaded RX 7900 XTX /
Mesa 26.2.5 / SDL X11 packages passed 30 authored/original-data input/UI/resource
executions, three owner cycles each, with empty stderr/zero workers; each of four
resource fixtures passed three cycles per mode. All fifteen release/sanitizer
capture pairs match, including the prior original-data hierarchy/input baseline.

Original queues now deliver hit/inset tracking, enter/leave, captured multi-button
release, wheel, relative callbacks and typed C++ drag/drop consumers. Preserve
logical-pixel/inverted-Y conversion, original raw-relative-to-DDC behavior, button/
modifier masks/event identities/timestamps, raw unsigned/finite preflight, owner/
current-context and traversal guards. Stationary focus refresh follows update
traversal and priority/inset changes; cancellation finishes exit cleanup even when
a release consumer throws. Defer deletion until traversal ends and retire focus,
capture and drag state on hide/disable/destruction/loss. Empty base Lua handlers
are selected; nonempty handlers remain unavailable before stack mutation.

Top-level occlusion/raising, title/layout movement and complete widget drag remain
open. Pooled Lua/XML/language/template factories, actual edit boxes and physical
IME/mouse acceptance, other Frame consumers, full signed ordered GlueXML plus its
post-creation digest gate, actual login and transport/gameplay remain open. The
source-graph/local fixture corrections and superseded attempts remain recorded;
no detector was disabled. No new physical-input, Windows runtime, macOS graphics
or server acceptance. M3 stays incomplete; preserve signatures, server revision
discrepancies and interactive user sign-in.

The [original top-level Frame package](FRAME_STACKING.md) in
[PR #60](https://github.com/RovxBot/wowdecomp/pull/60) is qualified at
`1d675153d9fb6a18f70feb46e5808ceec2749138`, merged as
`2e81a1978a5dc34b82db5ce6457b69950199a415`. The [durable record](frame-stacking-validation.json)
retains all four triggered implementation PR workflows: source audit, Linux GL,
CPU layout matrix and full signature-enabled Windows x64 compilation. Both Linux
GL configurations passed 38 checks; CPU layout also executed on actual ARM64 macOS
and Windows. Hosted checkout `d45d48d3efe6a995cbfdc19e6f1bc154bcb795ee`
has the same source tree as the selected implementation and merge. Three archive
digests, all 51 packaged executable hashes/types and notices verified.

Downloaded RX 7900 XTX / Mesa 26.2.5 / SDL X11 release and strict ASan/UBSan/leak
packages passed 30 input/UI/resource executions with three owner cycles each,
empty stderr and zero workers. Each target/transform/buffer/texture fixture passed
three cycles per mode. All fifteen release/sanitizer capture pairs match; final
original-data hierarchy/input captures retain the prior baseline. The inspected
640x480 original-data mouse capture is authored white text with original Friz
Quadrata/UI shaders, not original login rendering. Intermediate stacking buffers
assert red/green draw-order changes, repeat pixels and restoration; those buffers
are not exported captures.

Original same-stratum positive-rectangle occlusion, descendant exclusion, nearest
top-level ancestor raising, hidden registered level preservation and literal sparse
compression now run in the native adapter. Preflight final raised descendants
before list mutation. Retire dirty batches across every allocated level, including
old source levels above the compressed active range. Keep callback guards on both
sides of internal mouse-down raising and retain resize-callback guards. The first
hosted release failure (37/38, duplicate text-batch ownership after compression)
and superseded runs remain recorded; no ownership/pixel assertion or detector was
disabled. Windows-selected source tokens match the immutable pre-PR base.

Title/layout move/resize, pooled Lua/XML/language/template factories, complete
widget behavior and physical mouse/IME/edit-box/candidate acceptance remain open.
Full signed ordered GlueXML creation and its post-creation digest gate, actual
login rendering, transport/authentication and observed gameplay remain open.
No new physical-input, Windows runtime, macOS graphics or server acceptance.
M3 remains incomplete; preserve signatures, historical identities, server revision
discrepancies and interactive user sign-in.

The [original Frame movement package](FRAME_MOVEMENT.md) in
[PR #62](https://github.com/RovxBot/wowdecomp/pull/62) is qualified at
`e0540e9b194d71b26459b1d78c0832b910688de4`, merged as
`3b9f9f820905e04ce38a18f91661acbd6e4de7db`. [Durable validation](frame-movement-validation.json)
retains all thirteen passing implementation PR workflows, 38 CTest checks per
Linux release/strict ASan/UBSan/leak configuration and full signature-enabled
Windows x64 compilation at matching-tree checkout
`534d8eb6e229ba3f63fdb4aae0a19df85ce8765c`. Three archives/all 51 packaged binary
hashes/types/notices verified. Exact-source RX 7900 XTX / Mesa 26.2.5 / SDL X11
release and strict sanitizer packages passed 30 input/UI/resource executions,
three cycles each, with empty stderr/zero workers; all fifteen final capture pairs
match. Original signatures and the qualified GlueFonts subset remain enabled.

Original title-region hit movement, Alt layout moves, Control edge/corner resizing,
explicit C++ script-reason movement, nine-point DragBy, effective scale, resize
bounds and flatten/unflatten now use the native root. Title regions are direct
C++ parent-owned allocations, not pooled Lua/XML factories. Stop retires the
borrowed movement pointer before unflatten callbacks. Hide/disable/destruction,
host focus/capture loss and deferred deletion retire movement. Preserve guarded
callbacks and defer raising until resize traversal finishes. Attached-text movement
and exact repeats are asserted in intermediate framebuffer readbacks; final captures
remain authored hierarchy text, not original login rendering. No failed hosted/
runtime attempt or detector suppression was needed; duplicate push runs remain
separate from the thirteen passing PR qualification runs.

Pooled Lua/XML/language/template factories, remaining widgets, physical mouse/IME/
edit-box/candidate UI acceptance, complete signed ordered GlueXML creation and its
full post-creation digest gate, actual login rendering, transport/authentication
and observed gameplay remain open. No new physical-input, Windows runtime,
macOS graphics or server acceptance. M3 stays incomplete; preserve signatures,
historical identities, server revision discrepancies and interactive user sign-in.

The [pooled Frame allocation package](FRAME_POOLS.md) in
[PR #64](https://github.com/RovxBot/wowdecomp/pull/64) is qualified at
`d2088a7cab528ca915b5fb1faa3cb354048ebd0a`, merged as
`f3e74f19562583ded6c8aeb414f4dccf60782da0`. [Durable validation](frame-pool-validation.json)
retains thirteen passing PR workflows, 38 CTest checks per Linux release/strict
ASan/UBSan/leak configuration and full signature-enabled Windows x64 compilation
at matching-tree checkout `eb7985d3665dcbd81f9c1c40b9be4543938f15ab`. Three archives,
all 51 executable hashes/types and notices verified. RX 7900 XTX / Mesa 26.2.5 /
SDL X11 packages passed 30 three-cycle input/UI/resource executions, empty stderr
and zero workers; all fifteen capture pairs match. Four resource fixtures also
passed three cycles per mode. The initial restricted X11-unavailable launch remains
failed evidence; the same binary passed with desktop access and unchanged detectors.

Original `Create_SimpleFrame` and a typed native FontString constructor now use
original class pools. Preserve native slot alignment, exception rollback, mixed
heap/pool deletion and parent/root ownership. Pools register with the Fonts owner;
close all consumers/raw slots before it clears blocks. Growth to 101/810 slots,
reuse, throwing construction, recursive/deferred/root destruction, actual pooled
text rendering/repeat pixels and owner release are qualified. This does not install
Lua/XML registry/metatables, templates/language or remaining widget factories.
Route each selected allocator/deletion before enabling those callers. Original
signatures and before/after GlueFonts-subset interface checks stay enabled.

Region/frame/title/FontString metatables, named-parent/template/language processing,
nonempty Lua input handlers, remaining widgets and physical mouse/IME/edit-box/
candidate UI acceptance remain open. Complete signed ordered GlueXML and its full
post-creation digest gate, actual login rendering, transport/authentication and
observed gameplay remain open. No new physical-input, Windows runtime, macOS
graphics or server acceptance. M3 is incomplete; preserve historical identities,
signatures, server revision discrepancies and interactive user sign-in.

The [bounded original region Lua package](FRAME_SCRIPTS.md) in
[PR #66](https://github.com/RovxBot/wowdecomp/pull/66) is qualified at
`56aa592edfc7685c7234119aab874f55173bf61f`, merged as
`914a6d5b72a3edee185379559c5b56a2ebd9b273`. [Durable validation](frame-script-method-validation.json)
retains thirteen passing PR workflows, 38 CTest checks per Linux release/strict
ASan/UBSan/leak configuration and full signature-enabled Windows x64 compilation
at matching-tree hosted checkout `3c9d0ba7edc0a4bc9004907a3cdafe1919e18a62`.
Three archives/all 51 executable hashes/types/notices verified. RX 7900 XTX / Mesa
26.2.5 / SDL X11 packages passed 30 three-cycle authored/original-data desktop
executions, empty stderr/zero workers; all fifteen capture pairs match. Four
resource fixtures also passed three cycles per mode. All compilation ran in Actions.

Original region/Frame/title/FontString metatables now expose bounded identity,
parent/name, dimensions/anchors, hierarchy enumeration, visibility, title reuse and
pooled Lua `CreateFontString` with named-font inheritance. Preserve canonical live
Lua tables, finite coordinates, checked point indices and owner/current-context/
live-root/traversal admission. C++ failures unwind before Lua errors. Close every
region/script object before the Scripts owner, then renderer/Fonts/Gxu/layout/XML/
VM/device/context/queue. Original draw-layer converters moved unchanged to shared
FontXML helpers; Windows-selected legacy tokens match the immutable base.

Language/text setters and XML virtual templates remain explicit Lua errors. The
new rendering fixture uses authored C++ `process=0` text in Lua-created objects;
intermediate rendering/repeats are readback assertions, not exported new captures
or login composition. Original signatures and before/after GlueFonts-subset checks
stay enabled. A usage-limit approval-review failure interrupted one unexecuted
sanitizer attempt; resumed strict checks passed with unchanged detectors.

Original FrameXML registry/global `CreateFrame`, named-parent XML processing,
templates/inheritance/language, additional methods/nonempty Lua input handlers,
remaining widgets and physical mouse/IME/edit-box/candidate UI acceptance stay
open. Complete signed ordered GlueXML and its full post-creation digest gate,
actual login rendering, transport/authentication and observed gameplay remain open.
No new Windows runtime, macOS graphics or server acceptance. M3 remains incomplete;
preserve historical identities, signatures, server revision discrepancies and
interactive user sign-in.


The [original language/text-setter package](LANGUAGE.md) in
[PR #68](https://github.com/RovxBot/wowdecomp/pull/68) is qualified at
`f0f522c467177cb1c044758c0f686d3001e9f3a4`, merged as
`2a0e3f5105fcb81064d287ba515fec2639c20151`. [Durable validation](language-validation.json)
records thirteen passing implementation workflows, 38 CTest checks per Linux
release/strict ASan/UBSan/leak configuration and full signature-enabled Windows
x64 compilation at matching-tree checkout `fb6ba2fe87c3129ca078a8182c8ff952777c9ebf`.
Three archives/all 51 executable hashes/types/notices verified. RX 7900 XTX / Mesa
26.2.5 / SDL X11 release/strict sanitizer packages passed 30 three-cycle authored/
original-data desktop executions, empty stderr/zero workers; fifteen capture pairs
match. Four resource fixtures passed three cycles per mode.

Original Korean particle/rieul, French article/vowel and plural decisions now back
Lua `SetText`/positional `SetFormattedText`. Text numeric conversions are separate
from coordinate finite-float checks. Preserve 4095-byte valid UTF-8 intermediate
admission, grammar/article nouns, signed-32-bit counters, 64 passes, owner-thread
borrowed scratch buffers and C++ unwind before Lua errors. Region/direct-C++ and
Lua fixtures assert processing, unchanged text on rejection, retained caches,
rendered transformed authored text, exact repeats and teardown. Windows language/
renderer/setter tokens remain unchanged. The prior link and stale-fixture failures
remain evidence; detectors stay enabled. Original-data signatures stay enabled.

DB-backed declensions, XML registry/global `CreateFrame`/named-parent/templates,
remaining widget/nonempty Lua input consumers and physical IME/edit-box/candidate
acceptance remain open. Complete signed ordered GlueXML/full second digest gate,
full login rendering, transport/authentication and observed gameplay remain open.
No new physical-input, Windows runtime, macOS graphics or server acceptance. M3 is
incomplete; preserve historical identities, signatures, server revision discrepancies
and interactive user sign-in.


The [original base Frame XML package](FRAME_XML.md) in
[PR #70](https://github.com/RovxBot/wowdecomp/pull/70) is qualified at
`2932c2fdb7eac99bc1a583f3c4374e7ab49fc71c`, merged as
`70faf33a85e5a967530ea463d7da4f9d94fc7ba1`. [Durable validation](frame-xml-validation.json)
records thirteen passing final-head PR workflows, 38 CTest checks per Linux
release/strict ASan/UBSan/leak configuration and full signature-enabled Windows
x64 compilation at matching-tree checkout `da814becf9a5a35681062ad14263e82cc07820c1`.
Three archives/all 51 packaged executable hashes/types/notices verified. RX 7900
XTX / Mesa 26.2.5 / SDL X11 release/strict sanitizer packages passed 30 three-cycle
authored/original-data executions, empty stderr/zero workers; all fifteen capture
pairs match. Implementation, hosted checkout and merge share one source tree.

Original base Frame registry, Lua `CreateFrame` and FontString XML-template
consumers now use the qualified pools/hierarchy/rendering. Preserve transferred
nodes beyond XML tree close, original Frame sibling-parent precedence, exact
FontString inheritance keys and class-specific taint semantics. Exceptions unwind
locks/taint and allocated descendants, including explicit external parents.
Global name/parentKey assignments reject Lua assignment-hook reentry; template
and numeric/name admission stays bounded. Close consumers before Scripts and its
nodes/registry storage, then renderer/Fonts/Gxu/layout/XML/VM/device/context/queue.
Original Windows-selected code and signature defaults stay intact. The initial
no-ink fixture and FreeType empty-outline UBSan failure remain evidence; native-only
empty-copy guards have a focused regression and no detector was disabled.

This is authored XML/text composition with original or authored resources, not
full login layout. Texture/backdrop/animation/model and remaining widget factories,
actual nonempty Lua handlers, broader XML/template semantics, DB-backed declensions
and physical IME/edit-box/candidate acceptance remain open. Complete signed ordered
GlueXML/full post-creation digest gate, full original login rendering, portable
transport/authentication and observed gameplay remain open. No new physical-input,
Windows runtime, macOS graphics or server acceptance. M3 remains incomplete;
preserve signatures, historical identities, server revision discrepancies and
interactive user sign-in. The clean client installation remains read-only.


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
