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

A playable native Linux or macOS client is still in development. Use the
[issue tracker](https://github.com/RovxBot/wowdecomp/issues) for current blockers, acceptance criteria and
validation evidence, and [PORTING_PLAN.md](PORTING_PLAN.md) for strategy.

| Platform | Demonstrated scope | Remaining acceptance |
| --- | --- | --- |
| Linux x86-64 | Native assets/SDL/OpenGL/UI foundation; original ordered GlueXML creation with full post-creation digest and login activation | Complete original login/model/FFX frames and input, transport/authentication, character selection and world entry |
| macOS ARM64 | Actual ARM64 core/file/asset/packet/script/XML/layout fixture execution in Actions | Native game renderer, original application/startup, desktop input and gameplay on Apple silicon |
| Windows x64 | Full client compilation in Actions and operator-reported D3D9 passes | Complete runtime/data/server/capture baseline; retained OpenGL warped-scene and busy-cursor failures |

The immediate Linux path is [CPU dispatch integration #93](https://github.com/RovxBot/wowdecomp/issues/93),
[original material continuation #94](https://github.com/RovxBot/wowdecomp/issues/94), then
[complete login rendering/input #96](https://github.com/RovxBot/wowdecomp/issues/96). Read those issues for
each source revision's actual observed stop and next step, including work on
pending branches. Neither authored model pixels nor login activation establishes
a complete original login frame. Sign-in stays interactive when ready.

Durable references: [runtime foundations](docs/m1/RUNTIME_DESIGN.md),
[assets and shell](docs/m2/HANDOFF.md), [graphics/UI contracts](docs/m3/HANDOFF.md)
and [integrated original-client startup](docs/m3/INTEGRATED_CLIENT.md).
Native audio/movie playback and applied graphics options remain separate open
requirements. No native authentication or gameplay result is established.

## Getting builds and running diagnostics

**All client and dependency compilation for this fork runs in GitHub Actions.**
Download artifacts from a passing run and verify their manifests/hashes before
execution. Native artifacts include diagnostic/component executables and the
opt-in [integrated Linux startup](docs/m3/INTEGRATED_CLIENT.md), which is still
being connected to the complete original login UI. The full Windows client target
requires Windows and MSVC; native startup success does not establish gameplay.

| Build or check | Workflow |
| --- | --- |
| Full Windows x64 reference client | [Windows baseline](.github/workflows/windows-baseline.yml) |
| Portable runtime | [M1 core](.github/workflows/m1-core.yml) |
| Files, assets and asynchronous reads | [M2 files](.github/workflows/m2-files.yml), [M2 assets](.github/workflows/m2-assets.yml), [M2 async](.github/workflows/m2-async.yml) |
| Native SDL3 shell | [M2 shell](.github/workflows/m2-shell.yml) |
| Linux graphics, images, fonts and Frame probes | [M3 OpenGL](.github/workflows/m3-gl.yml) |
| Opt-in integrated Linux startup | [Integrated Linux client](.github/workflows/native-client.yml) |
| Portable packet/script/XML/layout components | [Packets](.github/workflows/m3-packets.yml), [Lua](.github/workflows/m3-lua.yml), [FrameScript](.github/workflows/m3-framescript.yml), [XML](.github/workflows/m3-xml.yml), [layout](.github/workflows/m3-layout.yml) |

The [shell guide](docs/m2/SHELL.md#build-and-execution) covers artifact verification
and native shell execution. For the current Linux Frame/pointer probe, extract a
verified `linux-gl` artifact and run from this checkout:

```sh
python3 Tools/Porting/run_native_gl.py \
  --artifact /path/to/extracted-linux-gl-artifact \
  --frame-mouse --data /path/to/3.3.5a/Data \
  --settings /tmp/wowdecomp-settings --cache /tmp/wowdecomp-cache \
  --output /tmp/wowdecomp-mouse.json --snapshot /tmp/wowdecomp-mouse.ppm
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

[Hosted build scheduling](docs/CI_BUILD_POLICY.md) uses pull-request validation,
main regression pushes and manual dispatch. Feature branches no longer also build
on push; all existing jobs, platform matrices and sanitizer settings remain.
Fixture-only corrections can reuse explicitly verified unchanged production inputs,
retaining their original build identities and rerunning the affected suite.


The [original tiled backdrop package](docs/m3/BACKDROPS.md),
[PR #80](https://github.com/RovxBot/wowdecomp/pull/80), renders the unchanged
AccountLogin field backdrop with original tooltip images on an authored Frame.
[Qualification](docs/m3/backdrop-validation.json) records hosted Linux release/
strict sanitizer graphics and four native asset configurations, the separate full
Windows reference build, verified packages and actual RX 7900 XTX original-data
checks. This advances the field visuals; EditBox text entry, complete signed
GlueXML/login execution and transport/authentication/gameplay remain open.
M3 is incomplete. Clean data remains read-only and sign-in stays interactive.

The [original MessageFrame package](docs/m3/MESSAGE_FRAMES.md),
[PR #81](https://github.com/RovxBot/wowdecomp/pull/81), adds native pooled message
widgets, private-font ownership and deferred row preparation for future EditBox
candidate consumers. [Qualification](docs/m3/message-frame-validation.json)
records hosted release/strict sanitizer and separate Windows builds, verified
packages and RX 7900 XTX original-font checks. EditBox text/candidate integration,
complete signed GlueXML/login, transport/authentication and gameplay remain open;
M3 is incomplete. Clean data remains read-only and sign-in stays interactive.
