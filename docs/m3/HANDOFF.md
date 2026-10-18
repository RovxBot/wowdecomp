# M3 Linux game integration — 2026-10-18

M3 is in progress. The first work package introduces a Linux native ABI host
boundary under Gx for SDL3-owned desktop OpenGL contexts. The full native client
gate remains in place. No original login UI, networking, authentication,
character selection or world entry has been established.

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
an FBO, checks known pixels for texture coordinates, depth, blending and clipping,
then presents into the window's pixel-sized drawable. It checks off-thread
presentation rejection and repeated context/window teardown. A dummy driver
negative fixture must report unavailable OpenGL. Hosted X11/Mesa software
rendering establishes fixture behavior, separately from a real GPU/desktop.
This is not a complete shader/material/model/terrain parity test.

Optional original data mode authenticates the original 12340 signaturefile and
loads `Interface\Glues\LoadingBar\Loading-BarFill.blp` using the existing MPQ,
patch/locale and registered-MD5 policy. It displays that texture through ARB
programs. A loading-bar asset is not the original GlueXML login UI.

All compilation runs in [M3 OpenGL Actions](../../.github/workflows/m3-gl.yml).
Build results and exact runtime identities will be retained in this directory
after hosted execution. No local client or dependency compilation is permitted.

After downloading a passing `m3-linux-gl-*` artifact, extract it outside the
repository and both installations. The launcher checks the binary hash and
uses X11 with a matching child EGL platform:

```sh
python3 Tools/Porting/run_native_gl.py \
  --artifact /tmp/extracted-m3-artifact \
  --data /mnt/games/3.3.5aClean/Data \
  --settings /tmp/wowdecomp-m3-settings --cache /tmp/wowdecomp-m3-cache \
  --frames 3600 --timeout 120 \
  --snapshot /tmp/wowdecomp-m3.ppm --output /tmp/wowdecomp-m3.json
```

Omit data/settings/cache for the synthetic quad. Use the sanitizer artifact with
`--sanitizers`; keep all detectors enabled and retain external-library failures.
Escape/window close exits; F11 toggles fullscreen. Operator checks: resize,
minimize/restore, focus changes and fullscreen must keep the image stable and
the drawable dimensions correct. Record actual GPU/driver, X11 versus Xwayland,
artifact/commit, captures and errors. This does not request credentials or a
server connection. M2 input/settings acceptance remains separate.

Next dependencies, in order:

1. Adapt the existing Gx device/shared services to the native ABI and the new
   context boundary; audit render-target transitions and legacy async callers.
   Do not pass native file tickets as `CAsyncObject` or use inspection geometry
   as a replacement for full M2 animation/material ownership.
2. Bring up unchanged 12340 shader version `0x10003`, original fonts, Lua,
   GlueXML/FrameXML and addon verification. Render the actual original login UI.
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
