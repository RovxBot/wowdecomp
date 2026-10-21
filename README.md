# wowdecomp

A C++ decompilation of the World of Warcraft **3.3.5a client** (`Wow.exe`, build
**12340**, June 2010), based on [SatyPardus/wowdecomp](https://github.com/SatyPardus/wowdecomp).
The source follows the original module and file layout, with `// OFFSET:` tags
identifying reconstructed functions' original binary addresses for comparison.

This fork is building native clients for **Linux x86-64** and **macOS ARM64
(Apple silicon)** while retaining **Windows x64** as the regression reference.
The goal is to preserve unchanged 3.3.5a data, protocol, gameplay, Lua APIs and addon
compatibility. The [4.3.4 decompilation](https://github.com/SatyPardus/wowdecomp15595)
is a reference for selected improvements; it does not change the 12340 baseline.
No game data is included in this repository.

[Join the community Discord](https://discord.gg/DsZMGUD8aT) to discuss other clients
and share what you build.

## Native port status

**Updated: 2026-10-21. Linux has a tested native rendering and UI foundation.
A playable native Linux or macOS client is still in development.**

| Platform | What is demonstrated | What still blocks a playable prototype |
| --- | --- | --- |
| **Linux x86-64** | Native SDL3 shell; original asset loading; OpenGL UI shaders, images and fonts; original Frame layout, hierarchy and text rendering; resize, keyboard/text/focus and IME event delivery | Complete original login UI and its Frame factories/consumers, portable networking/authentication, full world rendering and an observed server session |
| **macOS ARM64** | Native ARM64 builds and execution of core, file/asset/async, SDL shell, packet, Lua, FrameScript, XML/signature and layout fixtures in Actions | A game graphics backend and shader integration, complete application/UI/input integration, and desktop/gameplay validation on Apple silicon |
| **Windows x64** | Full client compilation in Actions; operator-reported D3D9 gameplay passes | Complete baseline captures and data/server/Warden evidence; the existing OpenGL warped-scene and busy-cursor failures remain recorded |

Linux diagnostic applications can already display original resources and exercise
parts of the original UI engine. The next visible milestone is the **actual
original login screen**, followed by interactive sign-in, character selection and
world entry. macOS shares the portable foundations, but the current Gx graphics
target is Linux-only. SDL shell presentation on macOS does not establish a working
macOS game renderer.

There is no measured percentage or delivery date for a playable port. Completed
components reduce the remaining work, but UI integration, networking and world
rendering still need substantial implementation and validation. The source
reconstruction figures below measure a different kind of progress.

## Work completed

The following capabilities are implemented and qualified within their documented
scope. Cross-platform component tests and Linux desktop/GPU observations are
identified separately in the linked reports.

| Area | Completed work | Details and evidence |
| --- | --- | --- |
| **Build and runtime foundations** | OS/CPU-aware CMake targets, allocation, logging, time, synchronization, threads and native startup/shutdown; Linux, actual ARM64 macOS and Windows fixture execution | [M1 foundations](docs/m1/FOUNDATIONS.md), [runtime design](docs/m1/RUNTIME_DESIGN.md) |
| **Files and original assets** | Host paths, read-only MPQs, original-key authentication and registered-member MD5 checks; representative DBC, BLP, M2, WMO and map inspection; retained asynchronous reads | [Files](docs/m2/FILESYSTEM.md), [assets](docs/m2/ASSETS.md), [async](docs/m2/ASYNC.md) |
| **Native application shell** | SDL3 window/input, clipboard, settings save/reload and joined shutdown; original-data Linux X11/Wayland execution and recorded operator input acceptance | [M2 handoff](docs/m2/HANDOFF.md), [shell](docs/m2/SHELL.md), [closeout](docs/m2/closeout.json) |
| **Linux graphics and images** | OpenGL context, original `0x10003` UI shaders, render targets, transforms, vertex/index buffers, sampled textures and bounded legacy Gx UI dispatch; BLP2 image Services, authored mip chains and atlas/loose-image lookup | [M3 handoff](docs/m3/HANDOFF.md), [Gx device](docs/m3/LEGACY_DEVICE.md), [images](docs/m3/IMAGES.md), [mips](docs/m3/IMAGE_MIPS.md), [image sources](docs/m3/IMAGE_SOURCES.md) |
| **Original fonts and UI rendering** | Friz Quadrata rasterization; original Gxu handles, text layout and caches; 18 original GlueFonts definitions; Frame anchors, font/name factories, FontString regions, retained batches and parent/layer/root rendering | [Gxu](docs/m3/LEGACY_GXU.md), [layout](docs/m3/LAYOUT.md), [font factories](docs/m3/FRAME_FONTS.md), [regions](docs/m3/FRAME_REGIONS.md), [batches](docs/m3/FRAME_RENDER.md), [hierarchy](docs/m3/FRAME_HIERARCHY.md) |
| **Scripts and interface verification** | Product-modified Lua VM, original FrameScript execution/events/secure handlers and saved-variable serialization; XML trees, signed GlueXML/FrameXML manifest verification and AccountLogin XML parsing | [Lua](docs/m3/LUA.md), [FrameScript](docs/m3/FRAMESCRIPT.md), [XML/signatures](docs/m3/XML.md), [Storm strings](docs/m3/LEGACY_STRINGS.md) |
| **Frame window and input delivery** | SDL size events update Gx/Gxu/layout; original priority queues receive captured key/repeat/up, committed Unicode, focus and retained IME composition/candidate/result events | [Resize](docs/m3/FRAME_WINDOW.md), [keyboard/text/focus](docs/m3/FRAME_KEYBOARD.md), [IME](docs/m3/FRAME_IME.md) |
| **Packet prerequisites** | Original `CDataStore` packet buffers, ARC4 and SHA1/HMAC adapted to the native ABI, tested on Linux, ARM64 macOS and Windows | [Packet contracts and validation](docs/m3/PACKETS.md) |

The latest qualified package, [Frame IME PR #55](https://github.com/RovxBot/wowdecomp/pull/55),
passed all **13 implementation PR workflows**, including **37 CTest checks in each
Linux release and strict sanitizer configuration** and full Windows x64 compilation
with signature checks enabled. Downloaded packages passed **26 runtime executions**
across IME/input/UI and prior-resource checks, with three owner cycles each, empty
stderr and zero remaining workers. Release/sanitizer captures match. Exact source,
build and artifact identities are in the [validation record](docs/m3/frame-ime-validation.json).

The real-GPU results cover the recorded **AMD RX 7900 XTX / Mesa 26.2.5 / SDL X11**
profile with ASan, UBSan and leak detection. Current Frame input checks use injected
SDL events; they do not establish physical IME/language or edit-box/candidate UI
acceptance. Earlier shell operator acceptance remains separate. Wayland shell
functionality is observed, with an external Fontconfig leak qualification retained
in the [desktop lifetime report](docs/m2/DESKTOP_LIFETIMES.md).

## What remains before a working prototype

For this project, a playable prototype means a native executable that uses
unchanged 12340 data, signs in, selects a character, enters the world, supports
basic play, saves settings and exits cleanly. A rendered test or successful build
does not close that milestone.

1. **Finish original UI creation and interaction.** Connect pooled Frame/region
   Lua/XML factories, templates/language handling, texture, animation/model and
   remaining widget consumers. Complete mouse hit testing/occlusion/drag and actual
   edit-box integration alongside the qualified keyboard/IME delivery.
2. **Run and render the signed original login UI.** Execute complete ordered
   GlueXML creation, retain the full post-creation digest comparison, and validate
   the real screen and its required shaders/models. Current original-resource text
   and font-definition tests cover a subset of that path.
3. **Implement portable transport and authentication integration.** Replace the
   Windows IOCP/WSA backends beneath the existing interfaces, preserving partial
   I/O, framing, cancellation and callback lifetimes. The packet/crypto prerequisites
   do not yet provide sockets, a connection or login.
4. **Validate world rendering and gameplay on Linux.** Complete model, animation,
   material, terrain, world-object and effect coverage as required; observe character
   selection, world entry and basic play on the target local server.
   Verify the live realm endpoint and Warden outcomes. The
   [server snapshot](docs/m3/SERVER_IDENTITY.md) records an image-tag/embedded-revision
   discrepancy and does not establish session compatibility. Sign-in will be
   interactive, with credentials kept outside the repository.
5. **Bring the application to Apple silicon.** Validate and select the maintained
   graphics/shader path, complete Metal rendering and ARM64 application integration,
   then test Retina scaling, input, audio/movies and gameplay on a real Mac.
   The proposed Gx/bgfx Vulkan/Metal direction remains a prototype/decision task.

Audio, movies, wider GPU/display coverage, long-session stability and packaging
are further requirements for a complete release. Optional 4.3.4 visual upgrades
follow the baseline and remain separate from portability work.

| Roadmap milestone | Current position |
| --- | --- |
| **M0 — Windows baseline and dependency audit** | Source/build audit complete; runtime/data/server evidence remains partial |
| **M1 — Portable core and lifecycle** | Complete for the defined core scope |
| **M2 — Original assets and native shell** | Complete for the observed Linux X11 acceptance profile |
| **M3 — Linux login and world entry** | In progress; graphics/UI/input prerequisites qualified, login/network/world gates open |
| **M4–M6 — Complete Linux, maintained graphics, macOS ARM64** | Ahead; shared ARM64 component work is already tested |
| **M7 / M8 — Selected enhancements / release stabilization** | Enhancements tracked separately; native release qualification remains ahead |

See [PORTING_PLAN.md](PORTING_PLAN.md) for milestone exit criteria and
[the current M3 handoff](docs/m3/HANDOFF.md) for implementation dependencies.

## Getting builds and running diagnostics

**All client and dependency compilation for this fork runs in GitHub Actions.**
Download artifacts from a passing run and verify their manifests/hashes before
execution. Native artifacts currently contain diagnostic/component executables;
the full client CMake target still requires Windows and MSVC.

| Build or check | Workflow |
| --- | --- |
| Full Windows x64 reference client | [Windows baseline](.github/workflows/windows-baseline.yml) |
| Portable runtime | [M1 core](.github/workflows/m1-core.yml) |
| Files, assets and asynchronous reads | [M2 files](.github/workflows/m2-files.yml), [M2 assets](.github/workflows/m2-assets.yml), [M2 async](.github/workflows/m2-async.yml) |
| Native SDL3 shell | [M2 shell](.github/workflows/m2-shell.yml) |
| Linux graphics, images, fonts and Frame probes | [M3 OpenGL](.github/workflows/m3-gl.yml) |
| Portable packet/script/XML/layout components | [Packets](.github/workflows/m3-packets.yml), [Lua](.github/workflows/m3-lua.yml), [FrameScript](.github/workflows/m3-framescript.yml), [XML](.github/workflows/m3-xml.yml), [layout](.github/workflows/m3-layout.yml) |

The [shell guide](docs/m2/SHELL.md#build-and-execution) covers artifact verification
and native shell execution. For the current Linux Frame/IME probe, extract a
verified `linux-gl` artifact and run from this checkout:

```sh
python3 Tools/Porting/run_native_gl.py \
  --artifact /path/to/extracted-linux-gl-artifact \
  --frame-ime --data /path/to/3.3.5a/Data \
  --settings /tmp/wowdecomp-settings --cache /tmp/wowdecomp-cache \
  --output /tmp/wowdecomp-ime.json --snapshot /tmp/wowdecomp-ime.ppm
```

This is a diagnostic, not a login client. It requires Linux x86-64, an X11 session
(including Xwayland), a suitable OpenGL compatibility driver, Python 3 and the
currently tested 12340/enUS data. Use the `linux-gl-asan` artifact with
`--sanitizers` and separate output/settings/cache paths for strict sanitizer runs.
Keep the entire data installation read-only and outside Git/ordinary hosted CI;
all writable roots and reports must be outside it. The
[clean-data inventory](docs/m2/clean-data-inventory.json) identifies the tested
reference; representative authenticated reads do not certify every archive as stock.

## Windows reference client

Windows x64 remains the full-client reference. Win32 is retained for comparison
with the original 32-bit layouts; the baseline workflow builds x64.

Runtime requires Windows 10 or 11, compatible 3.3.5a (12340) data and the Visual C++
runtime required by the artifact. The runner build environment uses Visual Studio
2022 with **Desktop development with C++**, MSVC v143, a Windows SDK and CMake
3.21+. Win32 additionally needs NASM and an external `DivxDecoder.dll` from the
original client in `ThirdParty/DivxDecoder/`; x64 uses its Xvid replacement.

| Target | x64 output | Win32 output |
| --- | --- | --- |
| `WowClient` | `WowClient64.exe` | `WowClient32.exe` |
| `Wow` | `Wow64.exe` | `Wow32.exe` |

Runner builds write client executables to `bin/Win/`. For runtime use, prepare a
separate staging directory outside the repository and reference installations:

1. Extract the verified x64 executables and companion `XvidDecoder.dll` into it.
2. Supply a separate copy of the matching data as `Data/`, then configure
   `Data/<locale>/realmlist.wtf` in staging for the intended compatible server.
3. Start `WowClient64.exe` (or `Wow64.exe`) from that directory.

`WowClient` opens a console and writes `WowClient_crash.log` on a crash. `Wow` is
the client without that logger. The [Windows runtime record](docs/m0/RUNTIME_SESSION.md)
distinguishes reported D3D9 passes, known OpenGL failures and outstanding evidence.

| CMake option | Default | Effect |
| --- | --- | --- |
| `WOWDECOMP_DISABLE_SIGNATURE_CHECKS` | `OFF` | When enabled, skips MPQ and `.toc.sig` verification for modified data/interfaces; baseline qualification keeps checks enabled |
| `WOWCLIENT_CRASH_LOG` | `ON` | Enables the `WowClient` crash logger |
| `WOWCLIENT_OUTPUT_OVERRIDE` | empty | Overrides the `WowClient64` / `WowClient32` executable name |

## Original source reconstruction

Historical upstream coverage, measured against build 12340 on **2026-10-15**:

| Measure | Address-tagged reconstruction | Total | Share of original |
| --- | ---: | ---: | ---: |
| Functions | 24,515 | 27,157 | 90.3% |
| Code bytes | 5,270,186 | 5,656,573 | 93.2% |

A function counts when its original address is tagged in the source. These figures
have not been remeasured for the native port and do not measure native readiness,
correctness or gameplay coverage. Totals exclude 658 functions identified by IDA
as compiler library code and 132 import thunks. Much of the untagged remainder is
compiler-generated code, C runtime helpers or third-party code linked from source
(including FreeType, zlib, PCRE and parts of FMOD Ex).

## Repository layout and contributing

| Path | Contents |
| --- | --- |
| `Engine/Source/` | Engine modules: Storm, Gx/Gxu, Model2, Frame/FrameXML, sound, Lua and native portability adapters |
| `WoW/Source/` | Game objects, world rendering, UI, networking, database and glue screens |
| `WoW/Common/` | Code shared by game modules |
| `ThirdParty/` | Dependencies linked from their upstream sources, with their existing notices |
| `Tools/WowClient/` | Windows client target and crash logger |
| `Tools/XvidDecoder/` | x64 replacement for the original 32-bit `DivxDecoder.dll` |
| `Tools/Native*/`, `Tools/Portable*/` | Native diagnostic applications and component fixtures |
| `Tools/Porting/` | Hosted build tooling, source audits and artifact/runtime validation |
| `docs/` | Milestone contracts, handoffs and durable validation records |

Read [AGENTS.md](AGENTS.md) and [PORTING_PLAN.md](PORTING_PLAN.md) before porting
work. Preserve original address provenance, compatibility and signature defaults;
record imported fixes and their attribution in [BACKPORTS.md](docs/BACKPORTS.md).
Use small synthetic fixtures in hosted CI and external original data for recorded
runtime checks. Documentation changes need reference/whitespace checks, not a
client rebuild.

## Credits and recognition

- [SatyPardus/wowdecomp](https://github.com/SatyPardus/wowdecomp) and its contributors,
  for the original 3.3.5a reconstruction on which this fork is based.
- [SatyPardus/wowdecomp15595](https://github.com/SatyPardus/wowdecomp15595), for the
  4.3.4 reference and selected improvement candidates.
- [arlyon/wowdecomp](https://github.com/arlyon/wowdecomp) and
  [RosemyneH/wowdecomp](https://github.com/RosemyneH/wowdecomp), for native-port
  references reviewed in [the reference audit](docs/m2/REFERENCE_REVIEW.md).
- [binana](https://github.com/thunderbrewhq/binana)
- [thunderbrew](https://github.com/thunderbrewhq/thunderbrew)
- [whoa](https://github.com/whoahq/whoa)
- TOMRUS, for the initial IDB.
- Everyone who has reported bugs and helped test builds.
- The WoW modding community and the authors of the bundled dependencies.
