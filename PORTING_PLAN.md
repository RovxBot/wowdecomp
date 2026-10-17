# Native 3.3.5a client implementation plan

Created: 2026-10-16. Updated: 2026-10-16 after the M0 audit. Source/dependency
inventory and the Windows x64 GitHub Actions build are verified. M0 remains partial:
native Windows gameplay/OpenGL observations and a verified stock dataset are pending.

Current evidence and handoff: [M0 baseline report](docs/m0/BASELINE.md),
[build manifest](docs/m0/windows-baseline.json),
[runtime session record](docs/m0/RUNTIME_SESSION.md) and
[backport ledger](docs/BACKPORTS.md). Repository agent instructions are in
[AGENTS.md](AGENTS.md).

Implementation branch: `native-port/m0-baseline`,
[draft PR #1](https://github.com/RovxBot/wowdecomp/pull/1). The source-audit workflow
also passed in [run 38846763847](https://github.com/RovxBot/wowdecomp/actions/runs/38846763847).
These changes have not been merged into `main`.

## Outcome and scope

Build a native World of Warcraft 3.3.5a (12340) client for Linux x86-64 and macOS
ARM64, retaining Windows x64 as the regression reference. Use the 4.3.4 (15595)
decompilation as a source of selected engine improvements and implementation
references. macOS x86-64 is a secondary target if hardware and dependency support
allow it; Linux ARM64 and 32-bit native ports are outside the first release.

The default experience must retain 3.3.5a network behavior, gameplay, data formats,
Lua API and addon compatibility. Initial native builds must work with an unchanged
3.3.5a data installation. Later visual features are optional and may use a separate,
documented asset pack without making Cataclysm data a baseline requirement.

Success means a native Linux executable and a native ARM64 macOS application that
can log into the intended compatible test server, enter the world, play, save
settings and shut down cleanly. Compilation, a login screen, running through Wine,
or an x86-64 executable translated on Apple silicon are distinct intermediate
results and do not establish that outcome.

## Source baselines and evidence

Freeze these revisions for the first comparison; update them deliberately and record
the reason when incorporating later upstream work.

| Role | Repository | Inspected revision |
| --- | --- | --- |
| Product source audited for M0, current fork | [RovxBot/wowdecomp](https://github.com/RovxBot/wowdecomp) | `cd29da38b9162419ee0fa29c6b16e07e10f6c4f0` |
| 3.3.5a upstream | [SatyPardus/wowdecomp](https://github.com/SatyPardus/wowdecomp) | Local fork revision above; upstream tip was not separately audited |
| 4.3.4 reference and donor, branch `w15595` | [SatyPardus/wowdecomp15595](https://github.com/SatyPardus/wowdecomp15595) | `959b0b85c5f55c565328d7c3ca33027a65e41327` |

The initial plan inspected `3adfc71e9f5b123bfc22d7f7d5738e1782de7f55`; M0 advanced
the product source baseline to `cd29da3`, which includes the existing tutorial
reverse-search bounds fix. Preserve that fix. The successful baseline build uses
harness commit `5fece2ca698c53680174a4b8c20948b5c484ab21`, with unchanged `cd29da3`
game source.

Use a separate reference checkout outside this source tree, or fetch individual
files at the pinned revision. M0 obtained and verified a separate scratch checkout;
see the baseline report for reproduction. Do not merge the donor branch into the
product wholesale or add game data to Git.

The inspected evidence is:

| Finding | Source | Implication |
| --- | --- | --- |
| The local README reports a playable Windows x64 client | [README.md](README.md) | Establish an observed baseline before relying on that claim |
| CMake rejects non-Windows platforms and defines Windows macros globally | [CMakeLists.txt](CMakeLists.txt) | Build selection needs restructuring |
| OS code, Storm and BlizzardCore contain Windows-specific implementations | [Os](Engine/Source/Os), [Storm](Engine/Source/Storm/Source), [BlizzardCore](Engine/Source/BlizzardCore/Source) | Porting only the `Os` module is insufficient |
| The process entry initializes Storm before CRT startup and uses an 8 MiB fiber stack | [WinMain.cpp](WoW/Source/Win64/WinMain.cpp) | Preserve initialization dependencies and establish suitable native stack behavior |
| TCP uses Windows I/O completion ports | [OsTcp.cpp](Engine/Source/Os/Win64/OsTcp.cpp) | Replace the I/O backend while preserving callback and lifetime semantics |
| OpenGL depends on WGL and legacy ARB programs | [OpenGL implementation](Engine/Source/Gx/CGxDeviceOpenGL/CGxDeviceOpenGl.cpp) | A Linux compatibility-context renderer is an initial route; modern rendering requires shader work |
| FMOD selects Windows sources; the movie decoder links a Windows library | [SoundEngine build](Engine/Source/SoundEngine/CMakeLists.txt), [decoder build](Tools/XvidDecoder/CMakeLists.txt) | Audio and movies need explicit dependency decisions |
| The donor includes D3D11, framebuffer support, depth caching and sun shafts | [Donor Gx](https://github.com/SatyPardus/wowdecomp15595/tree/959b0b85c5f55c565328d7c3ca33027a65e41327/Engine/Source/Gx), [SunShafts.cpp](https://github.com/SatyPardus/wowdecomp15595/blob/959b0b85c5f55c565328d7c3ca33027a65e41327/Engine/Source/ShaderEffect/SunShafts.cpp) | These are candidates to adapt, with dependencies still to validate |
| The local shader loader expects `0x10003`; the donor expects `0x10004` | [Local loader](Engine/Source/Gx/CGxDevice/CGxDevice.cpp), [donor loader](https://github.com/SatyPardus/wowdecomp15595/blob/959b0b85c5f55c565328d7c3ca33027a65e41327/Engine/Source/Gx/CGxDevice/CGxShader.cpp) | Renderer and shader assets cannot be swapped independently |
| The donor remains Windows-only and its OpenGL integration uses WGL | [Donor build](https://github.com/SatyPardus/wowdecomp15595/blob/959b0b85c5f55c565328d7c3ca33027a65e41327/CMakeLists.txt), [GlExt.h](https://github.com/SatyPardus/wowdecomp15595/blob/959b0b85c5f55c565328d7c3ca33027a65e41327/Engine/Source/Gx/CGxDeviceOpenGL/GlExt.h) | It does not supply the native Linux/macOS platform layer |
| The local x64 Warden handler is limited | [WardenClient.cpp](WoW/Source/Game/GameClient/Win64/WardenClient.cpp) | Verify compatibility with the intended server early |

Address-tag coverage is reconstruction evidence, not a gameplay or correctness
metric. Many donor files retain code from earlier builds; a different file hash or
a newer build number does not prove an improvement.

## Architecture decisions

These are implementation defaults and bounded investigations, not claims that the
dependencies or interfaces already exist.

| Area | Direction | Decision evidence required |
| --- | --- | --- |
| Build | GitHub Actions exclusively for client/dependency compilation; CMake, C++17, MSVC on Windows, Clang first on Linux and macOS | Explicit OS/CPU source selection and reproducible dependencies; Windows hosted baseline is verified, native jobs are future work |
| Windowing and input | SDL3 behind existing engine-facing interfaces | Events, text input/IME, relative mouse, focus, fullscreen, DPI and clipboard work on target hardware |
| Core services | Standard C++ and POSIX implementations behind `Os`, Storm and BlizzardCore interfaces | File, synchronization, timer and callback semantics match existing callers |
| Networking | POSIX sockets with a portable event-loop implementation first | Partial I/O, cancellation, ordering, reconnect and shutdown pass controlled tests; select epoll/kqueue or a maintained library only as needed |
| Initial Linux graphics | Adapt the existing OpenGL compatibility path | Validate required ARB extensions and matching 3.3.5a shader assets on representative drivers |
| Maintained graphics | Prefer a Gx adapter using bgfx with Vulkan on Linux and Metal on macOS | A representative UI, terrain, skinned-model and render-target/shader prototype passes before committing to a full migration |
| Audio | Preserve `SE2` behavior; investigate a portable FMOD output path first | Source/dependency availability, codec support, 3D mixing, streaming and device recovery; replace behind `SE2` if necessary |
| Movies | Build a portable decoder dependency or adapt another decoder behind the existing interface | Existing movie playback, audio sync and skip behavior; no Windows import library in native targets |
| CPU | Portable scalar reference functions, then architecture-specific acceleration where measured | SSE replacements preserve texture, animation and numerical behavior on ARM64 |

[SDL3](https://wiki.libsdl.org/SDL3/FrontPage) supports Windows, Linux and macOS.
[bgfx](https://bkaradzic.github.io/bgfx/overview.html) supports Vulkan and Metal;
its integration and shader conversion are still project work. If the graphics
prototype fails, record the specific gap and compare a direct Vulkan/Metal adapter
or SDL GPU before selecting an alternative. Avoid maintaining multiple new full
renderers during the initial port.

Preserve the engine-facing Gx abstraction where practical. Introduce explicit
shader/material translation and resource ownership at its boundary. The modern
backend must handle 3.3.5a shader permutations, fixed-function behavior, coordinate
and depth conventions, texture compression and render-target behavior. Choose
translation of existing programs or equivalent newly authored shaders based on
coverage and visual comparisons; record which asset versions each path accepts.

Keep OS identity, CPU architecture and pointer width separate. Existing
`WOWDECOMP_ARCH_64` guards must not select Windows or x86 instructions merely because
a target has 64-bit pointers. Remove platform types from shared headers gradually
using opaque state and narrowly scoped adapters. Preserve serialized layouts with
fixed-width fields and explicit decoding rather than forcing every native runtime
object to match the original Windows ABI.

## Milestones and exit criteria

The critical path is M0 -> M1 -> M2 -> M3 -> M4 -> M5 -> M6 -> M8.
An audited, independent correctness fix from M7 can land earlier; visual upgrades
follow a stable native baseline and the graphics support they need.

Client and dependency builds, including experimental build checks, run in GitHub
Actions. Source inspection, documentation validation and source-audit Python tests
may run locally. Download built artifacts to suitable hardware for runtime/GPU
tests; hosted compilation is not runtime sign-off. The initial workflows are
[Windows baseline](.github/workflows/windows-baseline.yml) and
[source audit](.github/workflows/m0-audit.yml).

### M0 — Verify the baseline and map dependencies

Status: source/dependency inventory complete and Windows x64 compilation verified
in [Actions run 38845726826](https://github.com/RovxBot/wowdecomp/actions/runs/38845726826).
The artifact ZIP digest, all three binary hashes and AMD64 machine types were
verified without executing the binaries. Runtime sign-off is pending.

- [x] Pin and inspect the product/donor revisions, including a separate donor checkout.
- [x] Inventory bundled source and binary dependencies, versions, platform services,
      startup, packed model offsets and donor differences beyond annotations.
- [x] Build both Windows x64 client targets in Actions and preserve logs/hashes.
- [x] Inspect the supplied client and intended server deployment sources read-only.
- [ ] Establish a separate verified stock 12340 dataset and actual live server identity.
- [ ] Observe native Windows D3D9/OpenGL world sessions, Warden behavior and fixed scenes.

The supplied `/mnt/games/ChromieCraft` has build-12340 executable metadata and enUS
configuration, but includes custom patches/proxy components and distinct MPQs whose
names collide on Windows. It is a modified compatibility candidate, not a proven
stock baseline. GrimGuzzler's local deployment sources identify AzerothCore with
playerbots and enable Warden; actual live state and session compatibility have not
been observed. Keep data, private session logs and credentials external. See the
[M0 report](docs/m0/BASELINE.md) and [runtime record](docs/m0/RUNTIME_SESSION.md).

1. Preserve the pinned 3.3.5a revision as a comparison point and obtain the pinned
   donor reference. Inventory library sources, binary-only dependencies, versions
   and supported architectures; keep local data and credentials outside Git.
2. Build and run Windows x64 with unchanged 12340 data. Exercise login, character
   selection, a world session and shutdown on the intended test server. Check the
   existing OpenGL path as well as D3D9 to expose shader/extension issues early.
3. Record OS, CPU, GPU, driver, compiler, client/data build and locale, test-server
   implementation/revision/settings and observed failures. Confirm any Warden
   requirements; do not infer them from a successful login alone.
4. Capture a small repeatable scene set: outdoor terrain, indoor world objects,
   characters, spell effects, water and UI. Record screenshots and frame timing.
5. Produce a dependency map for entry/startup, Gx, files/archive I/O, synchronization,
   network I/O, audio, movies, SIMD and optional Windows peripherals. Diff behavior
   and interfaces against the donor, discounting address/comment-only changes.

Exit: a reproducible Windows baseline or a documented pre-existing failure, a
named server/data test setup, and an evidence-backed dependency inventory. Missing
hardware or data prevents runtime sign-off, but source/build investigation can
continue. Native implementation must not silently treat an unverified baseline as
passing.

### M1 — Establish portable build and runtime foundations

Depends on: M0 source/dependency inventory. Windows regression checks start here.

Status: started on `native-port/m1-foundations` from M0 handoff `8846d9b`.
The first work package isolates Windows source/build selection and adds an
asset-free hash core target with hosted checks. See [M1 foundations](docs/m1/FOUNDATIONS.md)
for evidence and remaining runtime work. M1 exit criteria and M0 runtime sign-off
remain open.
The initial core checks passed on Linux x86-64, macOS ARM64 and Windows x64 in
[run 38848848241](https://github.com/RovxBot/wowdecomp/actions/runs/38848848241);
both Windows client executables built in
[run 38848848237](https://github.com/RovxBot/wowdecomp/actions/runs/38848848237),
at source commit `e24dcebfc6815646e0cd5d42e9ef916c8acbcc0c`.

1. Make compile definitions, linker flags, resource files and libraries conditional
   on OS. Replace recursive platform globs with explicit selection so Linux never
   compiles Win32/Win64 implementations. Address both `Wow` and `WowClient` targets.
2. Add OS/CPU detection, supported compiler checks, dependency pins and out-of-tree
   native build presets and Actions jobs. Retain working Windows x64 builds and Win32 layout checks
   where available. New commands become documented only when they exist.
3. Port allocation/alignment, error/log handling, atomics, mutexes/events, threads,
   thread-local storage, monotonic time and secure randomness. Inspect all three
   layers: `Os`, Storm and BlizzardCore. Preserve recursive-lock, timeout and
   notification behavior required by callers.
4. Audit pre-CRT initialization and global constructors. Implement a native entry
   lifecycle with valid initialization ordering and sufficient stack capacity;
   do not transplant the Windows CRT entry or assume fibers are required.
5. Isolate MSVC calling conventions, SEH, intrinsics, CRT helpers and Windows types.
   Audit LP64 versus LLP64 differences (`long`, `wchar_t`, varargs and layout).

M0 identified an additional constraint: x64 M2 arrays retain signed 32-bit relative
offsets and `OsAllocNear` currently provides nearby allocations with Windows virtual
memory APIs. Design a portable equivalent or explicit representation change before
M2 model loading; arbitrary replacement allocations are not safe. A small CPUID
initialization fix is queued separately in the backport ledger.

Exit: portable core libraries build on Linux, targeted core behavior checks pass,
and a minimal native executable initializes and shuts down cleanly. Windows x64
still builds. An unavailable Windows runner is recorded as an outstanding check.

### M2 — Load original assets and create a native application shell

Depends on: M1 core services.

1. Port filesystem and async file services, including BlizzardCore package I/O.
   Preserve archive lookup, patch/locale precedence and file/signature checks.
   Keep MPQ-internal names separate from host paths; handle separators, Unicode,
   case-sensitive disk lookup, cache/config locations and read-only game data.
2. Prove archive opening and representative DBC, BLP, M2, WMO and map-data decoding
   using unchanged 12340 data. Add small synthetic format fixtures where possible.
3. Add SDL window/input integration with close, resize, focus, keyboard/text,
   clipboard and relative mouse handling. Port the event/message pump through
   existing engine interfaces and respect platform main-thread requirements.
4. Separate optional hardware integrations such as NVAPI, SteelSeries and Logitech
   LCD code from required startup. Report unavailable features explicitly.

Exit: native Linux startup creates a usable window, reads required original data,
saves/reloads settings and exits without leaks or hanging worker threads. No game
assets need to be copied into the repository.

### M3 — Render the login screen and enter the world on Linux

Depends on: M2. The modern graphics/shader prototype can begin during this stage.

1. Replace WGL/window ownership and extension loading in the existing OpenGL
   backend with SDL integration. Keep compatibility-profile requirements explicit
   and detect missing capabilities with actionable errors.
2. Validate shader loading, UI/text, models, terrain, world objects, particles,
   shadows and water against M0 captures. Check clipping/depth, blending, gamma,
   texture formats and render-target transitions.
3. Replace IOCP behind the existing network interfaces. Preserve connection
   lifetimes, callback threads/order, framing, partial reads/writes, cancellation,
   error reporting and disconnect/reconnect behavior. Leave 12340 packet formats,
   authentication and serialization semantics intact.
4. Complete login, character selection, world entry and movement against the same
   server used for M0. Test zoning/teleports, combat, chat, inventory and logout.

Exit: a native Linux executable can complete an observed world session with
unchanged data. Audio or movie limitations must be listed; this milestone alone
does not qualify as the complete Linux release.

### M4 — Complete the Linux experience and select modern graphics

Depends on: M3; the graphics selection prototype must complete before M5 migration.

1. Implement and validate audio device output, decoding/streaming, positional sound,
   music, volume controls and device changes. Document capture/voice support
   separately according to server capability; silent placeholders do not count.
2. Port movie playback and portable crash diagnostics. Validate repeated startup,
   world transitions, device/resource recreation and clean shutdown.
3. Test SDL integration under X11 and Wayland, including fullscreen/windowed mode,
   mouse capture, alt-tab, DPI and multi-monitor behavior on available hardware.
4. Prototype the proposed Gx/bgfx adapter: a UI quad/text, terrain material, skinned
   character, representative shader permutations, compressed textures, depth and
   offscreen rendering. Exercise Vulkan and obtain an early Metal hardware smoke
   test so the selection addresses both targets.
5. Record the backend/dependency decision, shader strategy, minimum OS/GPU/toolchain
   requirements and remaining unsupported features. Keep the existing renderer as
   a selectable comparison path while the replacement matures.

Exit: Linux has an end-to-end playable baseline including sound, settings and
movies, and the maintained graphics direction has demonstrated representative
behavior on Vulkan and Metal. Report driver/session coverage actually tested.

### M5 — Implement the maintained renderer and ARM64 core support

Depends on: M4 graphics decision. CPU isolation and scalar implementations can
start earlier without waiting for graphics completion.

1. Complete the selected Gx adapter and material/shader coverage for unchanged
   3.3.5a assets. Translate legacy behavior explicitly; avoid copying the 4.3.4
   shader loader and assuming existing assets still work.
2. Adapt useful donor framebuffer, resource/state management and shader-permutation
   concepts with recorded provenance. Keep 4.3.4 asset semantics behind adapters.
3. Replace x86-only CPUID, SSE, assembly and assumptions about alignment or memory
   ordering. Start with correct scalar equivalents; add ARM acceleration only
   when profiling and parity checks justify it.
4. Exercise the maintained backend on Windows and Linux for comparison, with
   resize, focus changes, repeated world transitions and resource lifetime checks.

Exit: Vulkan renders the M0 scene set and gameplay correctly; ARM64 core and
dependencies compile with architecture-specific paths selected correctly. A Metal
hardware prototype exists, with full macOS application validation still in M6.

### M6 — Deliver a native macOS ARM64 client

Depends on: M5. macOS core compilation and SDL shell work can begin during M2-M4.

1. Add Apple Clang builds, macOS POSIX/platform adapters and an application bundle
   with documented external data discovery and writable user settings/cache paths.
2. Complete Metal rendering and platform details: main-thread lifecycle, Retina
   scaling, text input, keyboard mappings, relative mouse, fullscreen, clipboard,
   sound output and movie playback.
3. Validate the executable and every bundled dependency as ARM64 on Apple silicon.
   Inspect artifact architectures and run on actual hardware. Treat Intel macOS
   as a separate optional build/test matrix, not evidence of ARM64 readiness.
4. Run the same asset, gameplay, addon, reconnect and long-session checks as Linux.
   Record packaging/signing requirements; keep distribution credentials external.

Exit: the ARM64 application runs natively on a real Mac, completes the common
scenario suite with the maintained renderer, and records minimum supported macOS
and hardware versions. Cross-compilation alone does not close this milestone.

### M7 — Backport selected 4.3.4 enhancements

Depends on: a recorded baseline and the dependencies of each individual feature.

For every candidate, record the donor commit, paths/functions and tagged binary
build/address; the concrete benefit; dependency/format changes; destination adapter;
required assets; supported backends; tests/captures; default state; and disposition.
Maintain the [backport ledger](docs/BACKPORTS.md), initialized during M0 with a
source-confirmed CPUID initialization candidate. No backport has been applied yet.

| Candidate | Initial disposition | Evidence needed before landing |
| --- | --- | --- |
| Independent correctness/64-bit fixes | Investigate first | A demonstrated local defect or behavioral improvement, with a focused check |
| Framebuffer/depth-cache infrastructure | Adapt during M5 if useful | Correct ownership, formats, depth handling and equivalent behavior across backends |
| Sun shafts | First bounded optional visual candidate | Depth/mask/composite path, matching or newly authored shaders, captures, performance and clean off switch |
| Liquid/reflection changes | Later optional feature | 12340 geometry/material adapter, shader coverage and indoor/outdoor/underwater comparisons |
| Model batching, animation or streaming changes | Profile before selecting | Actual benefit and preserved M2/WMO/map behavior; newer code alone is insufficient |
| UI conveniences | Separate small changes | Existing Lua/addons behave correctly; required information is available from the 12340 server |
| Complete D3D11 renderer | Reference; no initial backport | It does not by itself provide Vulkan/Metal portability |
| Cataclysm networking/JAM, object fields and gameplay schemas | Excluded from baseline | Would change the compatibility target and requires a separately scoped project |

Exit per feature: its dependencies and provenance are documented; both native
targets support it or disable it explicitly; default-off visual upgrades preserve
the original baseline when disabled; required assets and costs are documented.
M8 does not require every candidate to be implemented.

### M8 — Stabilize and package the initial release

Depends on: M6 and complete Linux validation; only selected, finished M7 features
are eligible for inclusion.

1. Establish Windows x64, Linux x86-64 and macOS ARM64 build checks. Add focused
   unit/integration tests for portable services, parsers and asynchronous lifetimes.
   Use asset-free fixtures in ordinary CI and separate observed gameplay checks.
2. Run sanitizers on portable/native paths where supported and triage findings.
   Resolve new races, parser errors, use-after-free and shutdown issues introduced
   or exposed by the port; record pre-existing failures explicitly.
3. Run at least a two-hour session on each primary native target, covering world
   transitions, combat/effects, UI/addons, audio, movies, reconnect and logout.
   Capture representative AMD/Intel/NVIDIA Linux coverage as hardware permits;
   do not claim untested drivers or sessions are supported.
4. Compare frame-time distributions, loading time, resident memory and visuals
   against recorded baselines on comparable hardware/settings. Explain any accepted
   regressions and quantify the cost of optional effects.
5. Produce reproducible native packages, dependency notices, setup/build/run docs,
   data-path instructions and a tested-support matrix. Packages contain client
   code/dependencies and documented optional artifacts, with game data supplied
   separately. Record release signing/notarization as separate distribution work.

Exit: reproducible packages, passing applicable checks, observed sessions on both
native targets, documented limitations and a clear comparison to the Windows
baseline. No unresolved release-blocking data corruption, crash or hang remains.

## Validation contract

| Change area | Required evidence |
| --- | --- |
| Files and assets | Synthetic parser fixtures, archive/locale/patch precedence, representative real assets, settings on case-sensitive paths |
| Threads and async I/O | Timeout, recursion/notification semantics, cancellation, partial I/O, disconnect races, object lifetime and shutdown |
| Protocol | Known serialization vectors or controlled-server comparisons; authentication/world entry, reconnect and zoning on the named server |
| Rendering | Fixed scene/camera/settings captures with reasonable tolerances; depth/blend/gamma/texture/shader/resource checks on each supported backend |
| ARM64/SIMD | Scalar-versus-accelerated output, alignment and numerical checks; actual ARM64 execution |
| UI/addons | Text/IME, key bindings, mouse interaction, addon loading and the existing Lua API |
| Audio/movies | Output and streaming, positional/volume behavior, device change, playback/skip and audio/video sync |
| Documentation only | Local link/reference and whitespace checks; no client rebuild required |

Always distinguish source inspection, compilation, fixture tests, hardware rendering
and observed gameplay. Missing hardware, game data or a compatible server is an
explicit validation gap, not a passing result.

## First implementation work packages

Start with these reviewable changes; maintain milestone status as evidence arrives.

- [ ] Record M0 Windows gameplay/OpenGL evidence and the actual data/server setup.
- [x] Complete the initial dependency/platform inventory, including bundled audio codecs,
      archive I/O, CRT startup, optional peripherals and binary-only components.
- [x] Build Windows x64 in GitHub Actions and verify artifact/binary identities.
- [x] Add OS/CPU-aware CMake selection and a portable core build target without
      claiming a native full-client build prematurely.
- [ ] Implement core allocation, logging, time and synchronization adapters with
      focused behavior checks; preserve Windows baseline builds.
- [ ] Port file/archive access and add an asset inspection executable for 12340 data.
- [ ] Add the SDL shell and prove input/settings/startup/shutdown behavior.
- [ ] Adapt Linux OpenGL context creation and display the original login UI.
- [ ] Implement portable network I/O and enter the world against the M0 server.
- [ ] Complete sound/movie support and the Vulkan/Metal graphics decision prototype.

## Resources, uncertainty and planning envelope

Assume one experienced C++ developer working full time for initial planning; no
fixed delivery date is established. A 1-3 week M0/M1 investigation should replace
the broad estimate with a dependency inventory, build evidence and a shader
prototype scope. An initial playable Linux port is a multi-month effort; reliable
Linux plus macOS ARM64 support may take a year or more. These are provisional
planning ranges, not commitments or estimates derived from measured port velocity.

The user supplied a modified 12340 client at `/mnt/games/ChromieCraft` and identified
GrimGuzzler on Kubernetes. Its deployment-source revision/settings are recorded in
M0, while live server identity and stock data provenance remain unverified. Windows
compilation runs successfully in GitHub Actions. Before runtime sign-off we still
need verified unchanged data, a native Windows session, usable Linux GPU access and
an Apple silicon Mac for execution. Build toolchains run in Actions; runtime data
and credentials remain external. Minimum Linux
distribution, compiler, GPU and macOS versions are selected from dependency and
hardware evidence during M0/M4, rather than promised before testing.

The largest uncertainties are shader completeness/asset compatibility, hidden
Windows startup and synchronization assumptions, audio/codec portability, server
requirements and ARM64 numerical/concurrency differences. Resolve them with small
prototypes and behavior checks before broad subsystem replacements. Keep the
working renderer selectable and each donor backport independently reversible.
