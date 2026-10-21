# Original unparented font-string regions

This follows the qualified [retained batcher](FRAME_RENDER.md). Original
`CSimpleFontString` bodies move out of `CSimpleRender.cpp`, and pixel rounding
moves out of `CSimpleTop.cpp`. Windows keeps the same bodies. The native target
selects these consumers with `CScriptRegion`, `CSimpleRegion` and the original
font-attribute string overload. No donor or dependency is added.

This first region surface is direct C++ and unparented, with explicit anchors to
the qualified layout owner. Real Frame parent/layer lists, animation groups,
region Lua/XML factories, language processing, embedded textures and the actual
screen root remain visibly unavailable. Draw requires explicit submission;
`m_visible` remains false without a parent. This is a prerequisite for those
callers, not their replacement. No login screen or gameplay is established.

Regions live inside the font factories, Gxu, layout/XML/VM and device/context.
Their constructors initialize native font height, offsets, gradient storage and
`m_maxLines` before preserving masks. Native heap font strings use ordinary C++
allocation/deallocation; the original public Windows pool is not a native factory.
Close regions before the font factories. Batches retain both the Services block
and its borrowed font, so completed queue submission survives region replacement
or destruction. Finish rendering before font/region access or destruction;
callback mutation and foreign-thread access are rejected. Actual root deferred
object destruction remains future work.

`NativeFrameRegionsCheck` runs three owner cycles with actual font inheritance,
layout, block/font replacement, region destruction while queued, repeated pixels,
shadow calls, callback/thread guards and unsupported-operation rejection. Both
original Gxu screen invalidation and the font-string callback execute as direct
fixture injection, followed by scale invalidation. This is not SDL resize delivery.
Optional original-data mode authenticates GlueXML, constructs the unchanged
GlueFonts definitions and renders an authored string through an actual region.
It preserves the signature subset's second digest check; complete Frame creation
and `CGlueMgr::Resume`'s post-creation digest gate remain open.

All client/dependency compilation and CTest run in GitHub Actions, including
strict ASan/UBSan/leak checks and separate full Windows x64 compilation. Hosted
and downloaded original-data qualification is recorded below. The native batch
entry dispatches through the selected region implementation, keeping unselected
Frame/Texture RTTI out of lower targets while retaining vptr checks. Unavailable
metatables reject before Lua stack/taint/name mutation. Failed font replacement
returns the original failure result and preserves live block/font handles.

## Qualification

[PR #47](https://github.com/RovxBot/wowdecomp/pull/47) is qualified at
`01ac7deb547aa3216a92d78f93b6eb07e59eb9a0`, merged as
`259518fd512a7c9cf4f4dcd8a4e0b0998058f282`. The
[durable record](frame-regions-validation.json) retains all thirteen passing PR
workflows, 33 CTest checks per Linux release/strict ASan/UBSan/leak configuration
and full Windows x64 compilation with signatures enabled. Hosted checkout
`c71a91a609e2a301415cd46e81cfff84b826613e` has the same tree as the selected
implementation and merge. Three archive digests, all 49 packaged executable
hashes/types and notices verified. Prerequisites retain actual ARM64 macOS and
Windows CPU coverage; this graphics target is Linux only.

Downloaded exact-source release and strict sanitizer binaries each passed three
authored region, three original-data region and three each of Frame batch,
GlueFonts, image and Gxu cycles on RX 7900 XTX / Mesa 26.2.5 / SDL X11. All twelve
main executions exited zero with empty stderr and zero workers at shutdown.
Each native target/transform/buffer/texture fixture passed three cycles per mode.
Original-key authentication observed 28,948 signaturefile records; registered
member MD5 policy remained enabled. GlueXML authenticated before/after construction
of all eighteen original GlueFonts definitions in every original region cycle.

The actual region rendered the authored red string `AV Frame region`, with
explicit anchors and scale 1.25, using a selected original outlined font at its
original logical height. It produced 213 red-ink pixels per original-data cycle;
repeat readback and release/sanitizer capture bytes match SHA-256
`6e0dbb222f3ff87e5444c326a689c9a8100710966a7c3d8cb52ef7ba5dd51fb3`.
The 640x480 release capture was inspected. Image-regression loading-bar hashes
still match prior qualification. This is authored composition with original
resources, separate from original login layout or physical-input acceptance.

Two earlier release runs passed 32/33 checks: the fixture drew after shadow-offset
mutation without processing the original pending layout queue. Diagnostics showed
pending flags and a missing text block. The fixture now processes that queue;
the pixel assertion is unchanged. Earlier strict linking also exposed references
to unselected Frame/Texture and font-string RTTI. Native dispatch and branches now
select only complete classes; no detector was disabled. The failures and cancelled,
unqualified sanitizer run remain recorded. Local Python: 52 passed, one CMake
availability skip; 113 original Windows body comparisons and whitespace passed.
The two additional scalar comparisons moved unchanged into `LayoutMath.cpp`.
No client/dependency compiled locally.

Run a downloaded, manifest-verified package without local compilation:

```sh
python3 Tools/Porting/run_native_gl.py --artifact /tmp/verified-region-package \
  --frame-regions --data /mnt/games/3.3.5aClean/Data \
  --settings /tmp/region-settings --cache /tmp/region-cache \
  --output /tmp/region-runtime.json --snapshot /tmp/region.ppm
```

Use `--sanitizers` and separate writable roots for the sanitizer package. The
installation stays read-only. No new physical-input, Windows runtime, macOS
graphics or server acceptance is claimed. M3 remains open; preserve original
signatures, server revision discrepancies and interactive user sign-in.

## Next original callers

1. Select actual Frame parent/child and region/layer lists, animation ownership,
   root batches and deferred destruction. Native region heap allocation is not
   the original pooled factory; adapt actual factory allocation before selection.
2. Connect region Lua/XML methods, language processing and template inheritance;
   the C++ region surface currently rejects those paths. Select actual Frame
   texture factories/image modes and embedded-texture ownership against the
   qualified image/atlas service.
3. Wire real SDL size events to both original Gxu invalidation and font-string/
   Frame callbacks. Preserve original pending-layout processing before batching.
4. Execute complete signed ordered GlueXML/FrameXML creation and the full second
   digest gate in `CGlueMgr::Resume`, then render original login before interactive
   user sign-in. Transport/authentication and observed gameplay follow.
