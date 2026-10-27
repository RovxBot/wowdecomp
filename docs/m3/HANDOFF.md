# Linux graphics contracts and diagnostics

Use the [issue tracker](https://github.com/RovxBot/wowdecomp/issues) for current targets and evidence.
The [integration direction](../../PORTING_PLAN.md#current-integration-direction--2026-10-25)
prioritizes original-client startup over exhaustive standalone widget ordering.
The [integrated client guide](INTEGRATED_CLIENT.md) describes original Glue/model
startup; [#96](https://github.com/RovxBot/wowdecomp/issues/96) tracks complete login rendering and input.

## Graphics and UI contracts

| Area | References |
| --- | --- |
| Native Gx/state/resource boundary | [Legacy Gx audit](LEGACY_GX.md), [device](LEGACY_DEVICE.md) |
| Programs, targets and geometry | [Shaders](SHADERS.md), [render targets](RENDER_TARGETS.md), [transforms](TRANSFORMS.md), [buffers](BUFFERS.md), [textures](TEXTURES.md) |
| Fonts and image services | [Fonts](FONTS.md), [Gxu](LEGACY_GXU.md), [images](IMAGES.md), [mips](IMAGE_MIPS.md), [sources/atlas](IMAGE_SOURCES.md) |
| Script/XML/layout | [Lua](LUA.md), [FrameScript](FRAMESCRIPT.md), [XML/signatures](XML.md), [layout](LAYOUT.md), [language](LANGUAGE.md) |
| Frame ownership/rendering | [Font factories](FRAME_FONTS.md), [regions](FRAME_REGIONS.md), [hierarchy](FRAME_HIERARCHY.md), [pools](FRAME_POOLS.md), [render batches](FRAME_RENDER.md) |
| Frame API/factories/callbacks | [Lua methods](FRAME_SCRIPTS.md), [XML factories](FRAME_XML.md), [handlers](FRAME_HANDLERS.md), [textures](FRAME_TEXTURES.md) |
| Window and physical input path | [Window](FRAME_WINDOW.md), [keyboard](FRAME_KEYBOARD.md), [IME](FRAME_IME.md), [mouse](FRAME_MOUSE.md), [stacking](FRAME_STACKING.md), [movement](FRAME_MOVEMENT.md) |
| Qualified widgets | [Buttons](BUTTONS.md), [CheckButtons](CHECKBOXES.md), [backdrops](BACKDROPS.md), [MessageFrames](MESSAGE_FRAMES.md), [EditBoxes](EDITBOXES.md), [StatusBars](STATUSBARS.md) |
| Native references and networking | [Apple startup review](APPLE_STARTUP_REVIEW.md), [packets](PACKETS.md), [server identity](SERVER_IDENTITY.md) |

Each module guide links its machine-readable evidence. Fixtures, debugger traces,
original-data rendering, physical operator input and gameplay are separate claims.
Historical context-probe operator acceptance does not qualify a complete login UI.

## Context ownership and diagnostic execution

SDL owns the desktop OpenGL context. The Os window requests OpenGL 3.3
compatibility, RGBA8, depth24, stencil8 and double buffering before creation.
Gx checks the returned attributes, ARB vertex/fragment programs, multitexture,
VBO, S3TC, EXT FBO support and required procedures; missing requirements fail
visibly. All SDL/context/render operations stay on the process main thread.
Procedures and GL resources expire before the owning context/window. A worker
may load assets and deliver one completion to the owner thread.

The basic probe draws ARB-programmed geometry into an FBO and checks texture
coordinates, depth/blending, bounds, DXT1 upload and drawable presentation. It
checks off-thread presentation rejection and context/window failure recovery.
Hosted X11/Mesa software rendering is distinct from actual GPU qualification.
These fixtures do not cover complete model/material/terrain behavior.

Download and hash-verify a matching artifact from
[M3 OpenGL Actions](../../.github/workflows/m3-gl.yml), then extract outside the
repository and both data installations. For the original loading-bar asset:

```sh
python3 Tools/Porting/run_native_gl.py \
  --artifact /path/to/extracted-linux-gl-artifact \
  --data /mnt/games/3.3.5aClean/Data \
  --settings /tmp/wowdecomp-settings --cache /tmp/wowdecomp-cache \
  --frames 3600 --timeout 120 \
  --snapshot /tmp/wowdecomp-gl.ppm --output /tmp/wowdecomp-gl.json
```

Omit data/settings/cache for the synthetic quad. Use the matching strict artifact
with `--sanitizers`; keep every detector enabled. The launcher selects matching
X11/EGL-X11. X11 through Xwayland does not qualify a separate native Xorg or
Wayland graphics session. Expired artifacts require a new Actions build and
identity verification, not local compilation.

Escape/window close exits; F11 toggles fullscreen. Record actual GPU/driver,
session, artifact/commit, captures and errors for resize, minimize/restore, focus
and fullscreen observations. A displayed loading-bar texture is separate from
the complete original login screen and does not request a server connection.

## Historical evidence

[Context validation](context-validation.json) retains the original probe's exact
build/GPU/operator identities. Later per-module JSON records retain their own
historical runs, failures and comparison captures. The repeated chronology and
obsolete next-caller ordering formerly in this handoff remain in
[pinned Git history](https://github.com/RovxBot/wowdecomp/blob/6a18bde50e300fc34d7ed983a2ed1dc97ef76d58/docs/m3/HANDOFF.md).
