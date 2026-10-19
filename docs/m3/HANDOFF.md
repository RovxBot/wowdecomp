# M3 Linux game integration — 2026-10-18

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

The initial target is `WowNativeGl` and the executable is `NativeGlCheck`.
`linux-gl` and `linux-gl-asan` extend the M2 allowlist without linking legacy
`CGxDevice`, WGL, D3D, game or optional vendor SDK objects. The existing Windows
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
[PR #23](https://github.com/RovxBot/wowdecomp/pull/23). The
[texture record](texture-validation.json) retains all eight passing PR workflows,
19 CTest passes per Linux GL configuration, separate Windows compilation identity,
verified artifacts and exact-source RX 7900 XTX release/sanitizer resource and
original-data shader cycles. These passed with empty stderr and zero workers.
The atlas fixture is synthetic; legacy texture callbacks/cache and original
font/login rendering remain open. The user will sign in interactively when
native authentication becomes available.

The [packet prerequisite package](PACKETS.md) prepares original `CDataStore`,
ARC4 and SHA1/HMAC code for native transport integration, with explicit unaligned
wire accesses and bounded native ownership. Its separate Actions target is being
qualified; no sockets or server session are implemented by it. Source inspection
also identifies `WowConnectionNet`'s WSA event/worker dispatcher, in addition to
the Os IOCP transport, as required networking work. The original login/gameplay
gate and the external server revision discrepancy remain unchanged.

Next dependencies, in order:

1. Adapt the remaining Gx device, texture/buffer and state-cache services to the
   native ABI/context boundary, using the qualified scalar transform/raster
   contract; audit texture/texgen delivery and legacy async callers.
   Do not pass native file tickets as `CAsyncObject` or use inspection geometry
   as a replacement for full M2 animation/material ownership.
2. Extend unchanged 12340 `0x10003` shader coverage beyond UI; bring up original
   fonts, Lua, GlueXML/FrameXML and addon verification. Render the actual original login UI.
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
