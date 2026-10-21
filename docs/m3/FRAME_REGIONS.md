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
and downloaded original-data qualification will be recorded after validation.

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
