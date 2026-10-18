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

Next dependencies, in order:

1. Adapt the existing Gx device/shared services to the native ABI and the new
   context boundary; audit render-target transitions and legacy async callers.
   Do not pass native file tickets as `CAsyncObject` or use inspection geometry
   as a replacement for full M2 animation/material ownership.
2. Extend unchanged 12340 `0x10003` shader coverage beyond UI; bring up original
   fonts, Lua, GlueXML/FrameXML and addon verification. Render the actual original login UI.
   The Windows OpenGL warped-scene failure remains a known comparison failure;
   obtain fixed D3D9 and OpenGL captures before claiming visual parity.
3. Replace IOCP beneath the original network interfaces, with framing, partial
   I/O, cancellation and lifetime fixtures; preserve 12340 authentication and
   packet layouts. Observe character selection and world entry on GrimGuzzler.
4. Record live server/core/image/endpoints and Warden behavior externally. M0
   records identify desired deployment settings only; do not change server or
   cluster policy or disable client signature checks to qualify a session.

M3 exits only after an observed native Linux world session with unchanged data.
Audio/movie limitations and unavailable platform coverage must stay explicit.
