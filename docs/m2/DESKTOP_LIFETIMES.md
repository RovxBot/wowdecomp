# M2 desktop library lifetimes

The Linux baseline now links its required X11/Wayland/libdecor dependencies
normally through ELF `DT_NEEDED`. This preserves system-library process-global
state for the process lifetime. It fixes the X11 shutdown report exposed by SDL's
previous dynamic load/unload boundary without suppressing the detector or changing
the SDL version. The [controlled evidence](desktop-leak-triage.json) retains source,
workflow, binary hashes, loading options, runtime results and module attribution.
Final baseline verification is recorded in the [handoff](HANDOFF.md).
At final implementation `50a7d91a1794529da6bc094fcf58dc5435392033`, all five
hosted shell configurations passed 12 CTest checks, including native Windows x64
and actual macOS ARM64. The [closeout](closeout.json) also records exact-source
Linux release, two X11 sanitizer launches, eight control cycles, a passing Wayland
release smoke and the still-failing Wayland Fontconfig sanitizer qualification.

## Control and result

`SDLDesktopControl` links only the pinned SDL and ordinary platform/standard
libraries. The generated build graph rejects engine libraries and the native
runtime ABI; CMake build utilities such as Visual Studio's `ZERO_CHECK` are allowed.
The program creates a window, starts/stops text input, optionally presents a solid
color through SDL's software renderer, and destroys renderer/window before SDL
shutdown. It has no engine initialization, allocator, file worker or game data.
Repeated init/quit cycles exercise the same system-library lifetime boundary.

At source `bd847b65f93a112f54ac80c688ee1e7ec52e4a96`, hosted Linux release,
ASan/UBSan and the initial linked diagnostic each passed 12 CTest checks in
[run 39042544222](https://github.com/RovxBot/wowdecomp/actions/runs/39042544222).
The initial Windows audit rejected `ZERO_CHECK`; the follow-up distinguishes
build utilities from actual compiled dependencies and tests that engine linkage
is still rejected. This initial run is not claimed as a passing Windows matrix.

| Actual desktop check | SDL dynamic display loading | Normal ELF dependency ownership |
| --- | --- | --- |
| X11 SDL-only control with software renderer | 50,184 bytes / 914 allocations reported | Exit 0, empty stderr |
| X11 repeated SDL-only init/quit | Comparison not repeated | Eight cycles, exit 0, empty stderr |
| X11 original-data shell | Historical report reproduced by the control | Two launches, assets/settings verified, zero workers, exit 0, empty stderr |
| Wayland SDL-only control with software renderer | 123,350 bytes / 3,267 allocations | 123,222 bytes / 3,266 allocations |
| Wayland SDL-only control without a renderer | Comparison not repeated | Same 123,222 bytes / 3,266 allocations |
| Wayland original-data shell | Historical failure retained | 123,210 bytes / 3,266 allocations; settings/worker teardown completed |

ASan/UBSan and leak detection stayed enabled. `addr2line` supplied symbolization;
no detector suppression, alternate allocator, `dlclose` interposer or forced
Fontconfig/Pango global cleanup was used. Sampling the child mappings during its
lifetime attributes otherwise-unmapped X11 allocation PCs to libX11 and Mesa
Gallium. The linked control and shell retain their ordinary system dependencies
until process exit, rather than orphaning their state after unloading libraries.
This is a linkage/ownership change, not a claim that system caches are explicitly
freed by the engine.

## Wayland qualification

The remaining linked Wayland report has 105 allocation stack groups whose
immediate allocating module is Fontconfig. The same groups reproduce without
engine code and without a renderer, through libdecor's GTK/Pango decoration path.
It is therefore classified as a system decoration-library lifetime report,
separate from the native asset worker and application allocations. The small
aggregate difference between control and shell does not justify claiming identical
allocation contents or a clean Wayland sanitizer pass.

[SDL's leak-report guidance](https://wiki.libsdl.org/SDL3/HowToReportBugs#reporting-memory-leaks)
explains why system-library and driver allocations need separate attribution. The
[maintainer discussion](https://github.com/libsdl-org/SDL/issues/15244) identifies
similar Fontconfig/libdecor reports; our control results provide the local evidence.
No upstream issue or message was posted. No host library was replaced.

The recorded Linux X11 profile provides the clean sanitizer baseline. Wayland
physical-input acceptance is recorded separately in
[operator-acceptance.json](operator-acceptance.json); its Fontconfig report remains
visible in the coverage matrix for later desktop/package validation. Cocoa desktop,
Windows physical input and other Linux compositors/GPUs are outside this observation.

## Dependency and reproduction contract

`linux-window` and `linux-window-asan` now use linked display dependencies.
`linux-window-dynamic-asan` explicitly restores SDL's dynamic loading for comparison.
The runner records and checks all three loading options; the source pin and archive
hash stay unchanged. Windows and macOS dependency selection stay unchanged.

Linux runtime installation must provide X11 client/extensions, Wayland client/EGL/
cursor, xkbcommon, libdecor and EGL shared libraries even when one particular display
provider is selected. The exact observed ELF dependency names are in the JSON
record. This startup dependency tradeoff is intentional; these display libraries
are required platform services, not optional peripheral SDKs. Missing libraries
must produce a loader/setup failure, not silent success. Packaging later needs to
state distribution-specific package names and preserve notices.

All compilation runs in Actions. Download a passing artifact and verify the
control or shell against its manifest before local execution. For a control:

```sh
chmod +x /tmp/extracted-artifact/bin/SDLDesktopControl
SDL_VIDEODRIVER=x11 EGL_PLATFORM=x11 \
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1 \
/tmp/extracted-artifact/bin/SDLDesktopControl --software --cycles 8
```

Use the hash-verifying [shell wrapper](../../Tools/Porting/run_native_shell.py)
with `--video x11 --sanitizers` for original-data acceptance. No game data enters
the control or ordinary hosted fixtures. Raw diagnostic logs and mappings remain
external; the durable report retains summaries and hashes without user input text.
