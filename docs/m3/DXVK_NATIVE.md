# Linux SDL3 / DXVK Native prototype

This opt-in standalone target resolves the native D3D9 ABI/window-system question
for [#133](https://github.com/RovxBot/wowdecomp/issues/133), ahead of original
`CGxDeviceD3d` integration. It does not install a Gx device or replace the integrated
client renderer. The modern Gx comparison remains
[#134](https://github.com/RovxBot/wowdecomp/issues/134), with selection in
[#101](https://github.com/RovxBot/wowdecomp/issues/101).

## Build and execution

All compilation runs in [Native D3D9 DXVK prototype](../../.github/workflows/native-d3d9.yml).
The independent `Tools/NativeD3D9/CMakeLists.txt` selects Linux x86-64, SDL3 and
native D3D9 explicitly. No Wine, Windows DLL, GL context or backend fallback is
used. Release and ASan/UBSan configurations build **both dependencies and the
fixture**; leak detection and halt-on-error remain enabled. Hosted execution uses
Xvfb and the [pinned CI-only Mesa lavapipe](../../Tools/NativeD3D9/ci-driver.json)
26.2.5 driver, so it does not qualify physical GPU performance. This
software driver is Actions-built with OpenGL/GLX/EGL/GLES disabled and selected
explicitly by its ICD JSON. It is not shipped in the runtime package or imposed
on the user's machine. Its source/library hashes and compiler/LLVM configuration
are retained in the manifest/logs; Mesa and distribution LLVM are uninstrumented
external drivers, while SDL, DXVK and the fixture remain instrumented in strict
mode. A marked CI driver patch releases the retained whole-buffer reference
before replacing it with a bounded vertex-buffer wrapper; the patch, adapted
source and hashes are retained. Earlier distro and unmodified-driver leak failures remain tracked in
[#146](https://github.com/RovxBot/wowdecomp/issues/146), separately from fixture
success and real-hardware qualification.
Strict artifacts also include the matching shared Clang sanitizer runtime and
its distribution copyright notice; all consumers use that one runtime.

The [dependency lock](../../Tools/NativeD3D9/dependencies.json) records exact
commits and archive SHA256s for upstream DXVK v2.7.1, its four submodules, and
the existing SDL 3.2.28 pin. One source adaptation guards empty updates in
DXVK's public-domain SHA1 utility, preserving null/zero-length input semantics
without null-pointer arithmetic. A digest-vector fixture covers empty/abc and
split 63/64/65/130-byte inputs with interspersed empty chunks. Its patch, adapted
source and hashes are packaged. A second patch corrects const return qualifiers
in DXVK's two SDL3 display-mode imports. Compile-time assertions compare every
WSI import with SDL's actual declarations, preserving the indirect-call detector.
No sanitizer check is suppressed. Meson enables only D3D9
and SDL3 WSI. A shared SDL library gives DXVK and the fixture the same SDL/window
state. SDL links its display dependencies normally, retaining their process
lifetime. Notices, toolchain logs, executable/shared-library hashes, workflow
identity and a PPM capture are packaged in `manifest.json` and its artifact.

Download a successful artifact, verify its ZIP digest against the Actions API,
extract it outside the game installations, then run:

```sh
python3 Tools/NativeD3D9/run.py \
  --artifact /path/to/extracted-artifact --output /tmp/dxvk-runtime.json --visible
```

Use the matching strict artifact with `--sanitizers`. The launcher verifies every
packaged file and ELF architecture before execution. Cache, logs and captures go
beside the report, outside the artifact and both read-only reference installations.
No game data or credentials are needed. Actual device/driver and D3D9 caps are
reported by DXVK; frozen OpenGL limits are not imposed on this backend.

## Fixture and limits

Each of three complete SDL/window/device lifetimes creates two distinct textures,
multiplies them through fixed-function stages into a 64×64 color/depth target,
and asserts exact interior pixels. Farther black geometry must fail depth;
nearer transparent geometry must fail alpha testing. The completed target is
sampled onto the backbuffer and read back before presentation. Default-pool
resources are released before resize/Reset, and the scene is recreated and
checked at the resized drawable extent. COM resources retire before the window;
the window retires before SDL. The optional PPM captures the last resized frame.
Reported scene time includes resource creation and synchronous readback; it is
**not** a gameplay frame-time benchmark.

Original UI/fonts, nonuniform UVs, 12340 `0x10003` shader packages and permutations,
M2/skinning, terrain/WMO, compressed textures, blend equations, FFX, device-loss
recovery, original-data captures and Windows D3D9 comparison remain open. Resize
Reset is distinct from device-loss recovery. This target neither reads nor alters
signatures, agreements, protocol or server/Warden state. Complete login remains
[#96](https://github.com/RovxBot/wowdecomp/issues/96); its current independent
service stop is native ScanDLL disposition
[#131](https://github.com/RovxBot/wowdecomp/issues/131).

## Vulkan-only shutdown control

[#147](https://github.com/RovxBot/wowdecomp/issues/147) tracks the separate strict
RADV exit leak. `VulkanControl` links only the system Vulkan loader and C++ runtime;
it creates no SDL, DXVK, engine, surface, swapchain or graphics resources. The same
workflow builds release/strict control packages and runs headless loader,
instance-only, physical-device enumeration and logical-device creation/idle/destroy
stages. One and three complete instance lifetimes distinguish initialization from
per-lifetime growth. A missing-device case checks exception cleanup. The hosted
control explicitly selects the distribution lavapipe ICD and records its packages,
loaded ELF hashes/build IDs and process maps. Those results do not qualify RADV.

To qualify only this independent control, dispatch `native-d3d9.yml` with
`control_only=true`; unchanged D3D9 production inputs may retain their separately
identified evidence under the [build scheduling policy](../CI_BUILD_POLICY.md).
After verifying the downloaded control ZIP against its Actions API digest:

```sh
python3 Tools/NativeD3D9/vulkan_control.py run \
  --artifact /path/to/vulkan-control-asan --sanitizers \
  --stage enumerate --cycles 3 --output /tmp/vulkan-enumerate.json
```

Use `--stage loader`, `instance` or `device` for the other controls. Device mode
accepts `--device 1002:744c` to require a unique vendor/device match; otherwise it
uses the first enumerated device. `VK_DRIVER_FILES` may select an explicit system
ICD and is recorded with layer-selection variables. Use a fresh output stem for
each run. The launcher verifies all package hashes and ELF architecture, fixes
strict detector settings, removes preload/audit/suppression overrides and writes
reports outside both read-only installations. Before/after-destroy maps and
loaded-library identities attribute unloaded driver frames without keeping a
driver reference or changing unload. An assertion marker followed by a nonzero
exit remains a failed control. No driver cleanup, suppression or ownership fix
is applied by this diagnostic.

## Reference audit and next boundary

Original `CGxDeviceD3d`/`CGxDeviceD3d9Ex` directly own HWND/class/message state,
load `d3d9.dll`, call display/focus/gamma Win32 APIs, NVAPI/stereo, and
Reset/Present. Their graphics semantics should be retained while platform leaves
move behind SDL/native services. The first prototype intentionally does not
compile these Windows implementations or make common headers pretend to be Windows.

Reviewed Arlyon's 12340 reference at
[`24778cd77a6a217bbfdffccb88d9556a06b8b21d`](https://github.com/arlyon/wowdecomp/tree/24778cd77a6a217bbfdffccb88d9556a06b8b21d):
`Tools/DxvkSmoke/{Main.cpp,CMakeLists.txt,README.md}`,
`cmake/FindDXVKNative.cmake` and `docs/MacOSBringup.md`. Its SDL2 draw/readback
fixture informed the test sequence; no implementation body is imported. The
[existing Apple review](APPLE_STARTUP_REVIEW.md) retains the other two pinned
ports' startup/Metal findings.

Arlyon's macOS test requires `fbraz3/dxvk` at `8703715` and a separate
MoltenVK/Vulkan-loader stack; stock upstream DXVK was not qualified there.
Upstream's x86 SIMD/build assumptions and Vulkan requirements mean this Linux
artifact establishes no ARM64/macOS support. Apple feasibility remains a separate
Actions/hardware task under #133/#102. No macOS dependency or source is imported.

Next, adapt original D3D9 Gx platform ownership and connect its authenticated
shader/resource callers, then compare original-data scenes with #107. Keep the
archived OpenGL implementation and historical evidence intact.
