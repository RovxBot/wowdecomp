# wowdecomp

A C++ decompilation of the World of Warcraft 3.3.5a client (`Wow.exe`, build 12340, June 2010).

Join the discord for other clients and to show off some cool stuff you build.

https://discord.gg/DsZMGUD8aT

Every function is reconstructed from the original binary and keeps the address it came from in an
`// OFFSET:` tag, so each one can be compared against the original. The source tree follows the layout
of the original client (`Engine/Source/<Module>`, `WoW/Source/<Module>`, original file names).

x64 is the main target. Win32 still builds and is used to check struct layouts against the 32-bit
original.

## Native client roadmap

[PORTING_PLAN.md](PORTING_PLAN.md) describes the planned Linux x86-64 and macOS ARM64 ports,
with selected engine improvements from the 4.3.4 decompilation. A native playable client is not implemented yet.
Coding agents should read [AGENTS.md](AGENTS.md). The [M0 report](docs/m0/BASELINE.md)
records the dependency audit and Windows x64 Actions build. Operator runtime results
report D3D9 passes and matching OpenGL failures; data/server evidence remains partial.
Client and dependency builds for this porting effort run in GitHub Actions.
The [M1 foundations](docs/m1/FOUNDATIONS.md) provide an asset-free core-service
library and `NativeCore` initialization/shutdown executable, tested in hosted
Linux x86-64, macOS ARM64 and Windows x64 jobs. The full native game remains gated.
The [M1 core workflow](.github/workflows/m1-core.yml) also runs Linux ASan/UBSan
and provides NativeCore binaries plus evidence; no game data is needed for that tool.
M2 is complete for the observed Linux asset/shell baseline. It adds
[host-file/path services](docs/m2/FILESYSTEM.md), authenticated retail MPQs
and [original asset inspection](docs/m2/ASSETS.md), an [async file queue](docs/m2/ASYNC.md)
and a [native SDL3 shell](docs/m2/SHELL.md). Hosted semantic fixtures pass on all
three platforms, including Linux sanitizers and actual ARM64 execution. Downloaded
Linux binaries load the supplied read-only 12340/enUS data and display decoded
terrain pixels on X11 and Wayland. The [M2 handoff](docs/m2/HANDOFF.md) records exact
evidence, passed operator input acceptance and the clean X11 sanitizer baseline.
The separate Wayland Fontconfig qualification remains documented. These tools do not run the
login screen, world, networking or audio.

M3 begins with a separate [Linux SDL3/OpenGL context probe](docs/m3/HANDOFF.md),
built by [M3 Actions](.github/workflows/m3-gl.yml). It checks desktop compatibility
contexts and synthetic ARB shader/FBO rendering, with optional original glue
texture display. It does not yet run the original login UI or connect to a server.
The [native target owner](docs/m3/RENDER_TARGETS.md) adds retained offscreen color,
depth and cube resources with hosted and actual-GPU sanitizer evidence. The full
native device, original fonts/login UI, networking and gameplay remain open.
The [transform adapter](docs/m3/TRANSFORMS.md) now reuses scalar Gx/Tempest math
for normalized viewport/scissor and target projection, with original UI shader
constants and transformed-pixel checks qualified in hosted and real-GPU
release/sanitizer runs. It follows the merged target work.
The merged [buffer adapter](docs/m3/BUFFERS.md), following the transform work, now
submits retained PCT vertex/index resources through original UI shaders, with
hosted and real-GPU release/sanitizer checks. Full legacy device integration,
original fonts/login, networking and observed gameplay remain open; M3 is incomplete.
The [texture owner](docs/m3/TEXTURES.md) now also handles ordinary sampled assets,
packed font-atlas formats, mip/cube uploads and DXT resources with qualified
hosted/GPU checks. Its synthetic atlas fixture is separate from original text.
The [original font preview](docs/m3/FONTS.md) now renders unchanged Friz Quadrata
glyphs through the original UI shaders, with qualified hosted and real-GPU
release/sanitizer checks. The next [legacy Gx UI adapter](docs/m3/LEGACY_DEVICE.md)
connects actual global dispatch, texture callbacks, shader caches and streamed
batches to the native resource owners, with qualified hosted and original-data
GPU release/strict sanitizer checks. The [original Gxu integration](docs/m3/LEGACY_GXU.md)
now connects original handles, layout/glyph caches, Services text handles and
batched font consumers, with hosted and actual-GPU release/strict sanitizer
evidence. The separate [product Lua runtime](docs/m3/LUA.md) now preserves the
modified taint/profile APIs and original pool/libraries in native Linux, ARM64
macOS and Windows fixtures, including strict Linux sanitizers. Original
[FrameScript execution/events](docs/m3/FRAMESCRIPT.md) now use that runtime with
owner-thread lifecycle, secure handlers and retained source reads. Hosted native
fixtures and downloaded Linux release/strict sanitizer original-source cycles
pass. Original Frame/XML/image services/GlueXML/login, transport and gameplay
remain open. The [original XML/signature integration](docs/m3/XML.md) now adds
owner-thread original trees and filename-bound interface verification. Hosted
Linux, actual ARM64 macOS and Windows fixtures pass; downloaded Linux release
and strict sanitizer binaries authenticate original GlueXML/FrameXML content
and parse AccountLogin XML. The [file-backed image Services](docs/m3/IMAGES.md)
now deliver bounded BLP2 images through retained reads and original CTexture/Gx
handles, with hosted and actual-GPU release/strict sanitizer evidence. Actual
Frame factories/layout, broader image/async contracts and original login
execution/rendering remain open, followed by transport and gameplay.
The [authored image mip/readiness extension](docs/m3/IMAGE_MIPS.md) now qualifies
complete original BLP2 mip chains, explicit mip filters, local wait 2 and UI read
priority, with hosted and RX 7900 XTX release/strict sanitizer checks. Frame's
disk/atlas contracts and actual original login rendering remain open.

The [original Frame layout prerequisite](docs/m3/LAYOUT.md) now qualifies actual
anchor/resize/XML dimension consumers on Linux release/strict sanitizers, native
ARM64 macOS and Windows. Frame factories, disk/atlas image contracts and actual
original GlueXML/login rendering remain open; M3 is incomplete.

The [original Frame font/name factories](docs/m3/FRAME_FONTS.md) now connect
CreateFont, inheritance, XML attributes and Lua object lifetime to native Gxu.
Both Linux GL configurations passed 31 tests; original-data release/strict
sanitizer runs construct 18 GlueFonts definitions and render a selected font.
Actual regions/font strings/root, disk/atlas images and full original GlueXML/login
remain open. M3 is incomplete.

The subsequent [native image lookup/atlas package](docs/m3/IMAGE_SOURCES.md) now
supports Frame `0x17` eligibility, an optional read-only loose-image root, actual
shared 8 by 8 atlas sheets and default filtering. Both Linux GL configurations
passed 31 tests; full Windows x64 compilation and downloaded release/strict
sanitizer image/original-GlueFonts regressions passed. Marked atlas and disk-source
pixels are authored evidence; the original loading bar remains standalone.
Actual regions/font strings/root, Frame texture modes, broader image formats and
full original GlueXML/login rendering remain open. M3 is incomplete.

The [original Frame render batcher](docs/m3/FRAME_RENDER.md) now retains native
quad snapshots and texture/shader/font/block resources through consumption. Both
Linux GL configurations passed 32 tests; full Windows compilation and downloaded
release/strict sanitizer GPU checks passed with original resources. Actual
regions/Frame/root lifetimes and full signed GlueXML/login remain open; M3 is
incomplete.

The [original unparented FontString regions](docs/m3/FRAME_REGIONS.md) now connect
actual region/layout/font inheritance and retained text batching through a bounded
C++ surface. Both Linux GL configurations passed 33 checks; full Windows compilation
and downloaded release/strict sanitizer original-data GPU cycles passed. Frame
region Lua/XML/animation, SDL size delivery and full signed GlueXML/login remain
open.

The [original Frame parent/root integration](docs/m3/FRAME_HIERARCHY.md) now uses
actual parent/child, region/layer/strata lists, inherited visibility/alpha/scale
and original root batches through direct C++ ownership. Both Linux GL configurations
passed 34 checks; Windows compilation and downloaded original-data release/strict
sanitizer GPU cycles passed. Pooled factories, input/animation/texture consumers,
actual SDL size delivery and full signed GlueXML/login remain open. M3 is incomplete.

The [SDL display-size path](docs/m3/FRAME_WINDOW.md) now connects actual
programmatic window events to Gx dimensions, original Gxu/font and Frame callbacks,
and pending layout before rendering. Both Linux GL configurations passed 35 checks;
full Windows compilation and downloaded release/strict sanitizer original-data
GPU cycles passed. Captures match release/sanitizer; the probe now initializes
coordinate math in original Glue startup order. This does not qualify physical
input, independent high-DPI changes or original login. Pooled Lua/XML factories,
remaining Frame/input/texture/animation consumers and full signed GlueXML/login
remain open; M3 is incomplete.

The [SDL keyboard/text/focus adapter](docs/m3/FRAME_KEYBOARD.md) now delivers
committed Unicode scalars and captured key/repeat/up through original root priority
queues, with bounded native key storage and callback lifetime guards. Both Linux
GL configurations passed 36 checks; signature-enabled Windows compilation and
clean-data release/strict sanitizer GPU checks passed. SDL input is injected test
evidence; physical-input and IME/language acceptance remain separate. Pooled
Lua/XML factories, mouse/IME and other Frame consumers, full signed GlueXML/login
and transport/gameplay remain open; M3 is incomplete.

The [SDL IME adapter](docs/m3/FRAME_IME.md) now delivers retained composition,
candidate and result snapshots through original Frame priority queues, including
candidate-only focus cancellation. All thirteen workflows, 37 checks per Linux
GL configuration and clean-data release/strict sanitizer GPU regressions passed.
These are injected input checks; physical IME, actual edit boxes, pooled Lua/XML
factories, mouse/occlusion, full signed GlueXML/login and gameplay remain open.

## Progress

Native 12340 packet buffer/cipher [prerequisites](docs/m3/PACKETS.md) passed
separate hosted Linux, ARM64 macOS and Windows checks. They do not provide sockets or login.

The percentages below measure how much of the original binary has a counterpart in this source tree. They do
**not** say how much of the game works, and the missing part is not missing gameplay: the client builds into a
complete executable and the game is 100% playable. Bugs could still be hiding, though; a ported function can behave differently
from the original.

Measured against the 12340 binary on 2026-10-15. A function counts as ported when its address is
tagged in the source.

| | Ported | Total | Share of the original |
|---|---:|---:|---:|
| Functions | 24,515 | 27,157 | 90.3% |
| Code bytes | 5,270,186 | 5,656,573 | 93.2% |

The total leaves out 658 functions that IDA identifies as compiler library code and 132 import thunks.
Most of the untagged remainder is code the build gets elsewhere: compiler-generated code (static
initializers, template instances) that our compiler emits on its own, C runtime helpers, and third-party
libraries that are linked from their upstream sources instead of being decompiled (FreeType, zlib, PCRE,
parts of FMOD Ex).

## Requirements

- Windows 10 or 11
- Visual Studio 2022 with the **Desktop development with C++** workload (MSVC v143 and a Windows SDK)
- CMake 3.21 or newer (the one that ships with Visual Studio works)
- For Win32 builds only: [NASM](https://www.nasm.us/), on `PATH` or passed as `-DNASM_EXECUTABLE=<path>`, and
  `DivxDecoder.dll` from your 3.3.5a client copied to `ThirdParty/DivxDecoder/` (the build copies it next to the
  executables; the x64 build uses its own decoder instead)
- A World of Warcraft **3.3.5a (12340)** client. No game data is included in this repository.

## Building

For this porting fork, all client/dependency compilation runs in GitHub Actions.
Use the [Windows baseline workflow](.github/workflows/windows-baseline.yml) for
the legacy client and the [M2 shell workflow](.github/workflows/m2-shell.yml) for
the native diagnostic application. Download binaries from a passing run; native
runtime instructions and hash verification are in [SHELL.md](docs/m2/SHELL.md).
The following upstream Windows commands describe the runner build environment:

```powershell
git clone https://github.com/SatyPardus/wowdecomp.git
cd wowdecomp

# x64 (main target)
cmake -S . -B build/x64 -G "Visual Studio 17 2022" -A x64
cmake --build build/x64 --config Release --target WowClient --parallel

# Win32 (optional, needs NASM)
cmake -S . -B build/win32 -G "Visual Studio 17 2022" -A Win32
cmake --build build/win32 --config Release --target WowClient --parallel
```

In-source builds are refused, so keep the build directory separate (`build/` above).

The client is written to `bin/Win/`:

| Target | x64 | Win32 |
|---|---|---|
| `WowClient` | `WowClient64.exe` | `WowClient32.exe` |
| `Wow` | `Wow64.exe` | `Wow32.exe` |

`WowClient` is the client with a crash logger: it opens a console window and writes
`WowClient_crash.log` when the client crashes. `Wow` is the same client without it. You can also turn the
logger off in `WowClient` with `-DWOWCLIENT_CRASH_LOG=OFF`.

### Options

| CMake option | Default | Effect |
|---|---|---|
| `WOWDECOMP_DISABLE_SIGNATURE_CHECKS` | `OFF` | Skip the MPQ file and `.toc.sig` signature checks, so modified game data and interface files load |
| `WOWCLIENT_CRASH_LOG` | `ON` | Build `WowClient` with the crash logger |
| `WOWCLIENT_OUTPUT_OVERRIDE` | empty | Name the `WowClient` executable something other than `WowClient64` / `WowClient32` |

## Running

1. Copy the `Data` folder of your 3.3.5a client into `bin/Win/`, so you have `bin/Win/Data/`.
2. Point `Data/<locale>/realmlist.wtf` at the server you want to connect to.
3. Start `bin/Win/WowClient64.exe` (or `Wow64.exe`).

## Layout

| Path | Contents |
|---|---|
| `Engine/Source/` | Engine modules: Storm, Gx (Direct3D 9 / OpenGL), Model2, Frame/FrameXML UI, SoundEngine, Lua, ... |
| `WoW/Source/` | Game modules: objects, world rendering, UI, networking, database, glue screens |
| `WoW/Common/` | Code shared by the game modules |
| `ThirdParty/` | Libraries linked from their upstream sources |
| `Tools/WowClient/` | The `WowClient` executable target |
| `Tools/XvidDecoder/` | x64 movie decoder that replaces the original 32-bit `DivxDecoder.dll` |

## Special thanks

- [binana](https://github.com/thunderbrewhq/binana)
- [thunderbrew](https://github.com/thunderbrewhq/thunderbrew)
- [whoa](https://github.com/whoahq/whoa)
- TOMRUS, for the initial IDB
- Everyone who has reported bugs so far
- The WoW modding community
