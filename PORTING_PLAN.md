# Native 3.3.5a client implementation plan

Created: 2026-10-16. Updated: 2026-10-21. M0 source inventory and Windows
compilation are verified; operator Windows sessions report D3D9 passing and the
same OpenGL rendering/busy-cursor failures on M0 and M1. M0 remains partial because
complete data provenance, live server/Warden evidence and captures/timings remain
incomplete. M2 now identifies a separate clean-data candidate and authenticates
representative native reads; that does not retroactively verify M0 sessions.

[M0 PR #1](https://github.com/RovxBot/wowdecomp/pull/1) and
[initial M1 PR #2](https://github.com/RovxBot/wowdecomp/pull/2) are merged into main.
M1 core services and the minimal native lifecycle are complete and merged via
[PR #3](https://github.com/RovxBot/wowdecomp/pull/3), with passing hosted core, sanitizer
and Windows client regression checks. See [M1 foundations](docs/m1/FOUNDATIONS.md),
[runtime design](docs/m1/RUNTIME_DESIGN.md),
[upstream-fix review](docs/m1/UPSTREAM_REVIEW.md) and the
[M0 baseline report](docs/m0/BASELINE.md). Historical source/build identities remain
in the reports and manifests. Repository instructions are in [AGENTS.md](AGENTS.md).
M2 asset/shell exit criteria are met for the observed Linux X11 baseline, with
operator Wayland input acceptance and its separate system Fontconfig qualification.
See the [M2 closeout](docs/m2/closeout.json) and [handoff](docs/m2/HANDOFF.md).

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

2026-10-18 upstream maintenance deliberately integrates the 50 reviewed 12340
commits through `b1c71b8bd108823bca952f16c6f83a7ad33b9648` in
[PR #15](https://github.com/RovxBot/wowdecomp/pull/15). The
[review](docs/upstream/2026-10-18.md) and [commit/build record](docs/upstream/2026-10-18.json)
retain the shared ancestor, integration/merge identities, corrective follow-ups
and remaining Windows runtime checks. The M0 source/build identities above and
the 15595 donor pin remain historical references; they are not replaced by this
maintenance merge.

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

Status: **complete for portable core/build/minimal-lifecycle exit criteria**.
Final implementation `b818951` passed core/sanitizer
[run 38990308909](https://github.com/RovxBot/wowdecomp/actions/runs/38990308909) and
Windows client regression [run 38990308848](https://github.com/RovxBot/wowdecomp/actions/runs/38990308848). [M1 foundations](docs/m1/FOUNDATIONS.md) records
contracts, scope and evidence. Core services and the native lifecycle passed on
Linux x86-64, macOS ARM64 and Windows x64, including a Linux sanitizer configuration,
in [run 38989058328](https://github.com/RovxBot/wowdecomp/actions/runs/38989058328) at
`9ee86a565a26dda463ea47eedb99d0fe79dc28ea`. The full native game remains in later
milestones; M0 runtime/data/server sign-off remains partial.

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

Pre-implementation [native-reference review](docs/m2/REFERENCE_REVIEW.md),
2026-10-17: inspected RosemyneH `main` at `d91c854` and Arlyon `macos` at
`24778cd`. Prefer selected POSIX/SDL and file-provider concepts, adapted to our M1
runtime and SDL3 default. The review records source defects, external build/runtime
evidence and proposed work packages; no code is imported or M2 exit criterion met
by that review. Keep graphics alternatives as separate prototypes.

Status: **complete for the observed native Linux asset/shell exit criteria**.
Implementation merged in [PR #5](https://github.com/RovxBot/wowdecomp/pull/5)
(host files), [PR #6](https://github.com/RovxBot/wowdecomp/pull/6)
(authenticated packages/assets), [PR #7](https://github.com/RovxBot/wowdecomp/pull/7)
(async files) and [PR #8](https://github.com/RovxBot/wowdecomp/pull/8) (SDL3 shell).
The [handoff](docs/m2/HANDOFF.md) records hosted checks on Linux x86-64, macOS
ARM64 and Windows x64, Linux sanitizers and real X11/Wayland execution against
the separate clean 12340/enUS reference. Native startup, original-data reads,
settings reload and joined shutdown are observed. The operator confirmed the
requested physical-input checks passed on 2026-10-18, with matching final Wayland
runtime telemetry in [the acceptance record](docs/m2/operator-acceptance.json).
The [lifetime fix](docs/m2/DESKTOP_LIFETIMES.md) in
[PR #11](https://github.com/RovxBot/wowdecomp/pull/11) makes X11 the recorded clean
ASan/UBSan/LeakSanitizer baseline using normal ELF display-library ownership.
Final head `50a7d91a1794529da6bc094fcf58dc5435392033` passed actual original-data
startup, settings relaunch and eight SDL teardown cycles with empty stderr, plus
all five hosted shell configurations and the full Windows reference. The
[closeout](docs/m2/closeout.json) preserves exact identities. Wayland's remaining
Fontconfig report reproduces in an engine-free control without a renderer and is
retained as an external platform qualification, not a clean Wayland leak pass.
M2 acceptance applies to the observed X11 Linux profile; broader desktop coverage
remains explicit. The shell is separate
from the gated full game and does not establish rendered scene or gameplay parity.

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

Status: in progress through the [Linux OpenGL context work package](docs/m3/HANDOFF.md).
Its separate native ABI probe does not establish the original login UI or gameplay.
[PR #13](https://github.com/RovxBot/wowdecomp/pull/13) is merged; hosted GL/Windows
compilation, synthetic render checks and exact-source original-data GPU sanitizer
execution are recorded in the [context evidence](docs/m3/context-validation.json).
The [original shader service](docs/m3/SHADERS.md) in
[PR #17](https://github.com/RovxBot/wowdecomp/pull/17) now decodes unchanged
`0x10003` UI shaders, owns native ARB resources and executes them on the observed
Linux GPU with clean sanitizer cycles; [shader evidence](docs/m3/shader-validation.json)
separates compilation/fixtures, captured output and operator observations. On
2026-10-18 the operator confirmed all requested original-data shader-probe window
controls passed without crashes or corruption, with a matching clean-exit report.
The complete device/native ABI, fonts/GlueXML/login UI and networking/world steps
remain open. M3 is not complete.

The [native render-target owner](docs/m3/RENDER_TARGETS.md) in
[PR #20](https://github.com/RovxBot/wowdecomp/pull/20) is qualified at
`2eb6dd9ee61c15aa5b788a6f7856cdb2019e9b4c`, merged as
`4d74dd0c5330f92c3fc282329fc071c61b23f62a`. Its
[record](docs/m3/render-target-validation.json) preserves all eight passing
implementation PR workflows, 16 CTest checks per Linux GL configuration, separate
Windows compilation identity, and exact-source real-GPU release/sanitizer target
and original-data shader cycles. This advances resource ownership; the full
device/state/projection, original UI, native networking and observed world remain
open. The [read-only live server snapshot](docs/m3/SERVER_IDENTITY.md) identifies
runtime images/binaries externally while preserving the distinction between the
package tag, embedded revision, mounted policy and unobserved session behavior.

The [transform work package](docs/m3/TRANSFORMS.md) now connects scalar Gx/Tempest
matrices and normalized viewport/scissor to the native targets and original UI
shader constant path. [PR #21](https://github.com/RovxBot/wowdecomp/pull/21) is
qualified at `a1c43f820448cbb0b0c0eb2fda2ab59216687415`, merged as
`37bc74b492ce3f547d8b222676a7cc547e7727ab` after PR #20. Its [evidence](docs/m3/transform-validation.json) records all eight PR
workflows, 17 CTest passes per Linux GL configuration, full Windows compilation
and exact-source GPU release/sanitizer target/transform/original-shader cycles.
It does not complete the legacy device/cache, original fonts/login UI, networking
or observed world entry. M3 remains open.

The next [buffer/vertex work package](docs/m3/BUFFERS.md) connects retained
vertex/index storage and checked PCT submission to the shader/target/transform
probe. [PR #22](https://github.com/RovxBot/wowdecomp/pull/22) is qualified at
`42859739d5359cffc0b55f33ebfed789ff152dcd`, merged as
`fd0ae32545f6da42b4fa58572eb26953131324c7` after PR #21.
Its [evidence](docs/m3/buffer-validation.json) records all eight successful PR
workflows, 18 CTest passes per Linux GL configuration, full Windows compilation
and exact-source RX 7900 XTX release/sanitizer buffer/target/transform/original
shader cycles. Full legacy device/state/texture/async, fonts/login, networking
and observed gameplay stay open; M3 is not complete.

The [sampled-texture package](docs/m3/TEXTURES.md) is qualified at
`8e4fcde51d7962411d27c462655ad677f95aa23c` in PR #23, following merged
PRs #20–#22. Its [evidence](docs/m3/texture-validation.json) records all eight
passing workflows, 19 CTest checks per Linux GL configuration, full Windows
compilation and real-GPU release/sanitizer checks. Synthetic ARGB4444 atlas output
is separate from original font rasterization and login UI. M3 remains open.

The [original font preview](docs/m3/FONTS.md) is qualified at
`16c111a5f36864530a2109ff7228a78f68fb17fd` in PR #24. Its
[evidence](docs/m3/font-validation.json) records nine passing PR workflows,
20 CTest checks per Linux GL configuration, full Windows x64 compilation,
verified artifacts and strict original-data GPU font/UI-shader cycles. Original
Friz Quadrata rasterization is now observed; full legacy Gx/Gxu cache/layout and
GlueXML/login are still open. The [legacy integration audit](docs/m3/LEGACY_GX.md)
maps those remaining dispatch and ownership dependencies. M3 remains incomplete.

The [Storm string/cache prerequisite](docs/m3/LEGACY_STRINGS.md) in
[PR #26](https://github.com/RovxBot/wowdecomp/pull/26) adapts original hash/name
allocation and ordered formatting to the native ABI. Review also preserves
ordinary fixed-width `I64` GUID/achievement formats and caller-owned `va_list`
reuse. [Qualification](docs/m3/string-validation.json) at `2d94646` retains nine
passing PR workflows, verified artifacts and exact-source GPU regressions. Full
legacy device/Gxu dispatch, original login, transport and the observed world-session
exit criterion below remain open.

The [bounded legacy Gx UI dispatch adapter](docs/m3/LEGACY_DEVICE.md) in
[PR #27](https://github.com/RovxBot/wowdecomp/pull/27), originally stacked on PR #26, connects
common device/state/cache and actual texture/buffer/shader objects to the native
owners. It is qualified at `6ef31f4a94cef0d1b1947f46e4e35d716568b520` with
[nine passing implementation workflows](docs/m3/legacy-device-validation.json),
21 CTest checks per Linux GL configuration, full Windows x64 compilation and
exact-source original-data GPU release/strict sanitizer checks. Original Gxu
handle/layout/cache, remaining full-device contracts and the login/network/world
gates remain open.

PRs #26 and #27 are merged as `c3ef9fd989d15beb4af2695ea0cc08865e23bd19`
and `6dae38e729a44c8df2424a0f27d6b892fcac7b0f`. The subsequent
[original Gxu package](docs/m3/LEGACY_GXU.md) in
[PR #28](https://github.com/RovxBot/wowdecomp/pull/28) is qualified at
`ade47622c9023fee38fce1b6346be2f8be51fce4`. Original HFACE, Services text
handles, layout/markup, glyph/kerning/row/string caches and batched rendering now
use the concrete native adapter. Its [record](docs/m3/gxu-validation.json) retains
nine passing implementation workflows, 22 CTest checks per Linux GL configuration,
separate full Windows compilation and exact-source RX 7900 XTX release/strict
sanitizer cycles. Original-font output and authored eight-page eviction pressure
are separate evidence. Frame/Lua/XML/GlueXML, general image/async services,
transport/authentication, full world rendering and observed gameplay remain open.
M3 is not complete.

The [product-modified Lua runtime](docs/m3/LUA.md) in
[PR #29](https://github.com/RovxBot/wowdecomp/pull/29) provides native pool-backed
states, the original five libraries, taint and profiling prerequisites. The
Linux release/strict sanitizer, actual ARM64 macOS and Windows native fixtures
pass six CTest checks each. At that prerequisite checkpoint, original FrameScript
ownership, files/events/secure handlers, Frame/XML and login rendering remained
separate integration work.

The [original FrameScript integration](docs/m3/FRAMESCRIPT.md) in
[PR #31](https://github.com/RovxBot/wowdecomp/pull/31) now qualifies owner-thread
execution/events/secure handlers, retained source reads and saved-variable
serialization. Four native configurations pass 12 CTest checks each; downloaded
Linux release/strict sanitizer binaries execute unchanged original GlobalStrings
in six cycles each. This source check does not qualify interface TOC signatures,
Frame/XML factories, file-backed image services or original login rendering.

The [original XML/signature integration](docs/m3/XML.md) in
[PR #33](https://github.com/RovxBot/wowdecomp/pull/33) now qualifies native XML
ownership and original-key filename-bound TOC signatures. All twelve PR workflows
passed; four native configurations passed 14 CTest checks each, including actual
ARM64 and strict Linux sanitizers. Downloaded Linux release/sanitizer binaries
verified the original GlueXML/FrameXML content digests and parsed AccountLogin XML
in three cycles each. [Evidence](docs/m3/xml-validation.json) keeps exact identities
and failed attempts. Frame factories/image Services and actual GlueXML/login
execution/rendering remain open, followed by networking and observed gameplay.

The [authored image mip/readiness extension](docs/m3/IMAGE_MIPS.md) in
[PR #37](https://github.com/RovxBot/wowdecomp/pull/37) is qualified at
`41c0a78e7f25bf6a040a59560c5f33502297d6f9`, merged as
`0d90dcdd741e43418016aaeeb15a66c23be2f1f8`. Seven triggered PR workflows passed,
including full Windows x64 compilation and 23 CTest checks per Linux GL
configuration. Exact-source GPU release/strict sanitizer checks passed original
authored BLP2 mip minification, local wait 2 and UI priority, with verified artifacts
and empty stderr. [Evidence](docs/m3/image-mip-validation.json) remains separate
from earlier mip-zero image qualification. Original Frame layout/factories,
disk/atlas/TGA contracts and actual original GlueXML/login remain open; M3 is
incomplete and no gameplay/session acceptance is implied.

The [original Frame layout prerequisite](docs/m3/LAYOUT.md) in
[PR #39](https://github.com/RovxBot/wowdecomp/pull/39) is qualified at
`4aa176506079a35e81c3bf9d7108e7bff1d57df2`, merged as
`4cf5e0e536dca1bb96f06d8a8500e7ddd293c754`. Its [record](docs/m3/layout-validation.json)
retains thirteen passing implementation workflows, 15 CTest checks/three authored
layout cycles in each of four native configurations, full Windows x64 compilation
at matching-tree checkout `8e4de17077fd39972148f7b0aa1101a28e711dd6`,
five verified archives and all 51 executable hashes/types. Original layout/anchor,
resize-list and XML coordinate consumers now have native ownership, initialized
flags, bounded graphs and transitive rectangle invalidation. Downloaded Linux
release/strict sanitizer layout and original-data XML/signature regressions each
passed three cycles with empty stderr. Original Frame factories/login rendering
are still open; this adds no GPU, input, Windows runtime, macOS desktop or server
acceptance. Original signatures and interactive sign-in remain intact.

The [original Frame font/name factories](docs/m3/FRAME_FONTS.md) in
[PR #41](https://github.com/RovxBot/wowdecomp/pull/41) are qualified at
`c87df98ab5b8f05cab034941dece92a78f81d726`, merged as
`829904d849546a224201a885fdb4e1c55ce578e5`. The [record](docs/m3/frame-font-validation.json)
retains thirteen passing implementation workflows, 31 CTest checks per Linux GL
configuration and full Windows x64 compilation at matching-tree checkout
`ba54f09734f163123155e378048cadf3bcfde61b`. Three archives and all 45 packaged
binary hashes/types were verified. Exact-source RX 7900 XTX / Mesa 26.2.5 / SDL X11
release and strict ASan/UBSan/leak checks passed three authored movement/font,
three original GlueFonts and three original-data Gxu cycles each, with empty
stderr and zero workers. All 18 original GlueFonts definitions were constructed;
a selected small outlined font rendered at its original logical size, with exact
repeat pixels and matching release/sanitizer captures.

Native font/name factories now use the original Lua/XML/Gxu owners and canonical
live Lua tables. Native FreeType movement adapters preserve the original index
conversion and Windows paths; the strict prior failure and authored reproducer
remain in the record. Sample text/red tint are authored fixture inputs, separate
from original font bytes/definitions. Actual regions/font strings/root batching,
SDL size invalidation, disk/atlas images and full GlueXML/login remain open.
There is no new physical-input, Windows runtime, macOS graphics or server session
acceptance. M3 remains incomplete; signatures and interactive sign-in stay intact.

The subsequent [image lookup/atlas package](docs/m3/IMAGE_SOURCES.md) in
[PR #43](https://github.com/RovxBot/wowdecomp/pull/43) is qualified at
`e308d2e7eedc1e8f8cba7cfc8cec885e8fe5bfa6`, merged as
`dbb4a00d8c85b74ad11f34a8a5f68aa91692aa1e`. Its
[record](docs/m3/image-source-validation.json) retains thirteen passing workflows,
31 checks per Linux GL configuration, full Windows x64 compilation with signatures
enabled and three verified archives/all 45 packaged executable hashes/types.
Hosted checkout `e520213854a27c5fb2c694e3effbb0b9e3443821` matches the implementation
tree. Downloaded release/strict sanitizer checks passed eighteen authored image,
original-data image and original GlueFonts cycles in total, with empty stderr and
zero workers. Loading-bar captures match the prior qualification; its unmarked
BLP remains standalone. Shared-atlas/loose-source pixels are authored evidence.

The native service now accepts Frame `0x17` eligibility, explicit read-only loose
roots, original atlas coordinates/slot lifetimes and default filtering. The initial
atlas supports single-level clamped nearest/linear sources; repeat/mip and other
ineligible sources remain standalone. Actual regions/font strings/root, Frame
texture modes, TGA/streaming and full signed GlueXML/login remain open. The first
fixture's queued-read expectation failure remains recorded; no detector was
disabled. No new input, Windows runtime, macOS graphics or server acceptance.
M3 is incomplete; signatures, server discrepancies and interactive sign-in stay.

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
   The client also uses `WowConnectionNet`'s WSA event/worker dispatcher, which
   must be adapted alongside Os IOCP. The [native packet prerequisite target](docs/m3/PACKETS.md)
   is qualified separately in PR #25; it does not establish transport or login.
4. Complete login, character selection, world entry and movement against the same
   server used for M0. Test zoning/teleports, combat, chat, inventory and logout.

Exit: a native Linux executable can complete an observed world session with
unchanged data. Audio or movie limitations must be listed; this milestone alone
does not qualify as the complete Linux release.

The [original Frame batching prerequisite](docs/m3/FRAME_RENDER.md) is qualified
and merged via [PR #45](https://github.com/RovxBot/wowdecomp/pull/45). Actual
`CRenderBatch`/`CSimpleRender` bodies now retain native quad/resource lifetimes,
with full-width sorting and failure recovery. Thirteen PR workflows passed,
including 32 tests per Linux GL configuration and full Windows compilation.
Downloaded release/strict sanitizer GPU checks passed against unchanged clean
12340 data; the [record](docs/m3/frame-render-validation.json) preserves exact
identities, hashes and earlier fixture failures. Regions/Frame/root deferred
lifetimes, SDL size invalidation, texture modes and full signed ordered GlueXML/
login remain open, followed by transport/gameplay. M3 remains incomplete.

The [original unparented font-string regions](docs/m3/FRAME_REGIONS.md) are
qualified in [PR #47](https://github.com/RovxBot/wowdecomp/pull/47), selected head
`01ac7deb547aa3216a92d78f93b6eb07e59eb9a0`, merged as
`259518fd512a7c9cf4f4dcd8a4e0b0998058f282`. All thirteen workflows passed,
including 33 tests per Linux GL configuration and full Windows x64 compilation.
The [record](docs/m3/frame-regions-validation.json) retains matching-tree hosted
checkout `c71a91a609e2a301415cd46e81cfff84b826613e`, three verified archives/49
packaged binaries and clean release/strict sanitizer original-data GPU cycles.
Actual C++ regions now consume layout/font inheritance and retained batches;
Frame parent/layer/root traversal, region Lua/XML/animation, texture factories,
actual SDL size delivery and full signed GlueXML/login remain open. Callback
screen invalidation is injected evidence; no new operator or gameplay acceptance.
M3 remains incomplete; signatures and interactive user sign-in stay intact.

The [original Frame parent/layer/root integration](docs/m3/FRAME_HIERARCHY.md) in
[PR #49](https://github.com/RovxBot/wowdecomp/pull/49) is qualified at
`ac717d347e2226e06a4f36e0a2bd485f396f6ef2`, merged as
`9b01385d6a1bdb11a78905dd9c302d559cbc142d`. The
[record](docs/m3/frame-hierarchy-validation.json) retains thirteen passing PR workflows,
34 checks per Linux GL configuration, full signature-enabled Windows x64
compilation at matching-tree checkout `960ebca2d4bb7a50a7cd8ddd53f3e44a89f02274`,
three verified archives and all 51 packaged binary hashes/types. Downloaded
release/strict sanitizer binaries each passed three authored and three original-data
hierarchy cycles, plus region/batch/GlueFonts/image/Gxu and all four resource
regressions, with empty stderr and zero workers. Captures match; original-resource
text/layout is authored, not login. The retained fixture failure and UBSan enum-load
finding were corrected with all detectors enabled.

Actual parent/child, region/layer/strata lists, inherited visibility/alpha/scale and
root batches now use original Frame consumers through a direct C++ heap-owned
surface. Preserve `m_isShown` initialization before parenting, unsigned native
strata preflight, bounded parent/level/scale graphs, dirty-batch retirement before
rebuilding and update/collection/resize/draw guards. Original `m_destroyed` expires
attribute permission; the separate deferred object queue coalesces requests and
retires recursively deleted descendants. Close root consumers before renderer/
font factories/Gxu/layout/XML/VM/device/context/queue. Pooled Lua/XML factories,
animation/input/texture consumers and actual SDL size delivery remain open. The
complete signed ordered GlueXML plus post-creation digest gate and original login
remain open, followed by transport/gameplay. No new physical-input, Windows runtime,
macOS graphics or server acceptance. M3 stays incomplete.

The [SDL display-size integration](docs/m3/FRAME_WINDOW.md) in
[PR #51](https://github.com/RovxBot/wowdecomp/pull/51) is qualified at
`b173d38012681e61780c6d60f6dc069782220304`, merged as
`e36987aa8e58502708e29696a535e4d841a0dde9`. Its
[record](docs/m3/frame-window-validation.json) retains thirteen passing workflows,
35 checks per Linux GL configuration and full signature-enabled Windows x64
compilation at matching-tree checkout `ccdb724540ca225850355886b7c69b19ff564767`.
Three archives and all 51 packaged binary hashes/types verified. Downloaded release
and strict ASan/UBSan/leak packages passed eighteen authored/original-data window,
hierarchy and prior-resource executions, with three owner cycles each, empty stderr
and zero workers; all four resource fixtures passed three cycles per mode.

Actual programmatic SDL size events now refresh Gx dimensions and coordinate
aspect, invalidate original Gxu fonts and Frame regions, deliver the original
size callback and finish pending layout before batching. Preserve the current
context/owner and traversal guards, unhandled-event caller ownership, logical
versus drawable dimensions and deferred destruction. Initialize original coordinate
math before fonts/layout, as `CGlueMgr::Resume` does. The prior capped-font and
startup-order fixture failures remain recorded with unchanged assertions/detectors.
Release/sanitizer captures match; the corrected startup baseline moves historical
hierarchy text up one pixel, retained separately from PR #49's capture identity.
This is authored layout with original resources, not login. Physical keyboard/mouse/
focus/IME, independent high-DPI/platform coverage, pooled Lua/XML factories,
texture/animation/model consumers, complete signed ordered GlueXML and its full
post-creation digest gate, login and transport/gameplay remain open. No new
physical-input, Windows runtime, macOS graphics or server acceptance. Preserve
signatures, server revision discrepancies and interactive user sign-in; M3 stays open.

The [SDL keyboard/text/focus adapter](docs/m3/FRAME_KEYBOARD.md) in
[PR #53](https://github.com/RovxBot/wowdecomp/pull/53) is qualified at
`75225260de3b03a17b2f280ab75f377a9f295d42`, merged as
`74eeee2399937ed72da27f308e9d66ba370d8f23`. Its
[record](docs/m3/frame-keyboard-validation.json) retains thirteen passing PR
workflows, 36 checks per Linux GL configuration, signature-enabled full Windows
x64 compilation at matching-tree checkout `c2f06f3b20e3335f976682bb28fc1f709828663c`,
three verified archives and all 51 packaged binary hashes/types. Downloaded RX
7900 XTX release/strict ASan/UBSan/leak packages passed 22 authored/original-data
keyboard/window/hierarchy and prior-resource executions, three cycles each, with
empty stderr/zero workers; all four resource fixtures passed three cycles per mode.
Captures match the prior window baseline and each other.

Original root stratum/priority queues now deliver native key-down, captured
repeat/key-up, committed Unicode scalars and focus callbacks. Native storage
covers the full key enum, including Super keys; validate raw indices before
array/shift access. Preserve original modifier bits, event identities/timestamps,
PrintScreen fallback, hide/disable/destruction cleanup and traversal guards.
Native priority nodes use matching allocation/deallocation. Callback consumers
may defer deletion; finish input before root mutation/update/render/destruction.
Nonempty native Lua input handlers still reject before stack mutation until actual
pooled Lua/XML factories are selected. Authored SDL_PushEvent checks are not
physical-input or IME/language acceptance. Mouse/occlusion/drag/IME, pooled
factories, texture/animation/model consumers, full signed ordered GlueXML plus
post-creation digest gate, actual login and transport/gameplay remain open.
No new physical-input, Windows runtime, macOS graphics or server acceptance.
M3 stays incomplete; preserve signatures, server revision discrepancies and
interactive user sign-in.

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
- [x] Implement core allocation, logging, time and synchronization adapters with
      focused behavior checks; preserve Windows baseline builds.
- [x] Port read-only retail file/archive access and add a validated 12340 asset inspector.
- [x] Add the SDL shell, original-data startup, settings reload and joined shutdown.
- [x] Record operator acceptance of the requested physical-input checklist; IME language/display coverage remains limited.
- [x] Establish the clean X11 sanitizer baseline and disposition the reproduced external Wayland Fontconfig report.
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
compilation runs successfully in GitHub Actions. A separate read-only reference at
`/mnt/games/3.3.5aClean` now has a hashed 12340/enUS inventory and authenticated
native asset reads. Linux X11/Wayland shell execution is observed on the local
desktop. Before full-game sign-off we still need complete data/server provenance,
captured Windows scene evidence, native Linux game rendering and an Apple silicon
desktop session. ARM64 core/asset/input fixtures execute in Actions, separately
from that desktop requirement. Build toolchains run in Actions; runtime data
and credentials remain external. Minimum Linux
distribution, compiler, GPU and macOS versions are selected from dependency and
hardware evidence during M0/M4, rather than promised before testing.

The largest uncertainties are shader completeness/asset compatibility, hidden
Windows startup and synchronization assumptions, audio/codec portability, server
requirements and ARM64 numerical/concurrency differences. Resolve them with small
prototypes and behavior checks before broad subsystem replacements. Keep the
working renderer selectable and each donor backport independently reversible.
