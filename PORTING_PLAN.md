# Native 3.3.5a client implementation plan

Created: 2026-10-16. Updated: 2026-10-26. M0 source inventory and Windows
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

## Current integration direction — 2026-10-25

The user confirmed the deployment model: unchanged clean 3.3.5a/12340 game data
used by our compiled native Linux executable or macOS ARM64 application. The
reconstructed original client supplies UI and gameplay logic. Both supplied
reference installations remain read-only; settings, cache and logs live outside
them. Game data stays outside Git and ordinary hosted CI.

Prioritise bringing up the existing client as an integrated application. The next
observable result is original engine/client startup displaying and accepting input
in the complete original login UI, followed by native authentication, character
selection and world entry. The earlier component work remains useful, but its
widget-by-widget qualification sequence is no longer the default work queue.
This direction supersedes historical next-caller ordering below and in module
handoffs. It changes planning, not the recorded completion or evidence of any
milestone; full native startup/login/gameplay remain unestablished.

### Implementation approach

- Connect the original startup, CVar/settings, Event loop, resource Services,
  Frame/FrameXML/FrameScript and Glue callers through the existing native services
  and engine-facing interfaces. Select broader coherent source groups where
  practical. Continue the opt-in integrated native client target through Actions and
  resolve concrete compile/link/startup failures along its actual call path.
- Implement missing widgets, model/animation consumers and other reachable
  dependencies as needed by original login startup, in reviewable batches.
  Complete standalone coverage of every widget/style/language is not a gate to
  attempting that startup. Record any specific unavailable caller that blocks it;
  do not substitute authored UI composition for the original login screen.
- Permit bounded source compatibility helpers for existing CRT/platform interfaces
  where semantics, fixed-width types and ownership are understood. Reuse original
  module behaviour and qualified native facilities before introducing another
  independent service/owner/probe. Preserve established alignment, allocation,
  thread, callback and teardown corrections; integration does not discard them.
- Audit existing native ports for reusable implementations before independently
  implementing equivalent platform, transport or graphics work. Review each import
  for source/dependency provenance, notices, ABI, data/shader compatibility and
  Windows regression impact. Record imported paths and revisions in
  [the backport/native-reference ledger](docs/BACKPORTS.md). A reference review is
  not authorisation for a wholesale merge or a claim of compatibility.
- Start macOS integration as its dependencies and hardware permit. M4-M6 retain
  their release acceptance requirements, but do not prohibit earlier native Apple
  startup or a reviewed Metal backend. Keep a Linux-capable graphics path; Apple's
  Metal implementation cannot directly serve Linux. Evaluate the existing Metal
  implementations alongside the proposed maintained renderer before choosing
  further graphics work.
- Keep required Actions checks, full Windows regression compilation and strict
  sanitizer coverage. Run focused regressions for changed contracts and exposed
  failures, with validation batched around coherent integration changes. Additional
  probes should resolve an integration uncertainty or protect an actual contract;
  a separately qualified package for every widget/module is not required.

### Immediate deliverables and acceptance

1. Continue the Actions-built integrated Linux startup target using the existing
   SDL3/Gx path, original archive mounting, settings, Event and client lifecycle.
   Identify reachable missing interfaces and adapt them in coherent source groups.
   [PR #84](https://github.com/RovxBot/wowdecomp/pull/84) now selects original
   Model/ModelFFX, Model2 scene/cache/shared loading, Camera, ShaderEffect and FFX
   sources. Factory and failure-path checks do not establish original model or
   login rendering; retain the [integrated execution record](docs/m3/INTEGRATED_CLIENT.md).
   Clean-data execution now completes ordered GlueXML creation and matches the
   full post-creation digest. Original Glue build/account/CVar services, typed
   model/movie callbacks and actual video/no-audio-backend queries are connected;
   see the [service continuation and exact stop](docs/m3/INTEGRATED_CLIENT.md#original-glue-service-continuation--2026-10-26).
   Continue original login activation, then observe Model2/material/FFX paint.
   Audio/movie playback and graphics-option application remain unavailable;
   M3 stays open.
2. Execute the complete original ordered GlueXML/Include/Script creation path,
   retaining archive/member verification and the full post-creation interface
   digest comparison. Render the actual original login screen, exercise its input
   and verify shutdown with downloaded release and strict sanitizer artifacts on
   the read-only clean data. This result does not require a server sign-in.
3. Adapt the original active network dispatch, DNS, framing and authentication
   callers, with loopback checks for partial I/O, callback ordering, cancellation,
   reconnect and teardown. This work can proceed alongside UI integration; inspect
   actual startup/session use before deciding which Os IOCP interfaces are needed.
4. With the login path ready, let the user sign in interactively to GrimGuzzler.
   Observe authentication, advertised realm endpoints, Warden outcomes, character
   selection and world entry against the then-current server identity. Preserve
   the server's policy and the distinction between its tag and embedded revision.

Progress reports should name the integrated target, what its actual execution
reaches, and the next concrete blocker. Keep component fixtures, hosted builds,
original-data rendering, physical input and server/gameplay evidence distinct.
Unavailable services fail visibly. Original signatures and 12340 protocol behaviour
remain required; this direction does not introduce unchecked authentication,
fabricated integrity responses or changes to cluster/server/Warden policy.

### Inspected Apple integration references

These are additional 12340 implementation references, separate from the historical
M0 product source and pinned 15595 donor. Inspection does not qualify their binaries
on our hardware/data/server or import their code into this fork.

| Reference snapshot | Useful implementation direction | Evidence limits |
| --- | --- | --- |
| [jerrywossion/wowdecomp `apple` at `7489b5f2df6628a434b2ad9aceed66c0f2e6a431`](https://github.com/jerrywossion/wowdecomp/tree/7489b5f2df6628a434b2ad9aceed66c0f2e6a431) | Original client/Glue integration through Apple platform services; direct Metal with recognised original shader families; native transport and focused probes | README reports local original-client/world execution with ASan/UBSan/Metal validation. Documented local login omits executable challenge verification, requires existing `StrictVersionCheck=0` and does not implement native Warden. It does not qualify unchanged GrimGuzzler policy. |
| [fmustafayaman/WoWReMac `WowRe` at `3c7f82c02fa9d4c8399b8f95d30ef0df575ef069`](https://github.com/fmustafayaman/WoWReMac/tree/3c7f82c02fa9d4c8399b8f95d30ef0df575ef069) | Full original client target, bounded Mac compatibility headers/platform replacements and D3D9-bytecode-to-Metal translation | Source and published scene measurements support integrated native rendering work. Warden and Epoch integrations belong to separate branches; their results do not qualify the default branch or our server. |

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
| Core services | Standard C++ and POSIX implementations behind `Os`, Storm and BlizzardCore interfaces; bounded source compatibility helpers may retain shared callers | File, synchronization, timer and callback semantics match existing callers; preserve native widths and ownership |
| Networking | POSIX sockets with a portable event-loop implementation first | Partial I/O, cancellation, ordering, reconnect and shutdown pass controlled tests; select epoll/kqueue or a maintained library only as needed |
| Initial Linux graphics | Adapt the existing OpenGL compatibility path | Validate required ARB extensions and matching 3.3.5a shader assets on representative drivers |
| Maintained graphics | Compare reviewed existing Metal Gx implementations/shader strategies with a bgfx Vulkan/Metal adapter and other bounded alternatives; retain Linux graphics support | A representative UI, terrain, skinned-model and render-target/shader prototype passes before committing to a full migration; assess reusable native-port code first |
| Audio | Preserve `SE2` behavior; investigate a portable FMOD output path first | Source/dependency availability, codec support, 3D mixing, streaming and device recovery; replace behind `SE2` if necessary |
| Movies | Build a portable decoder dependency or adapt another decoder behind the existing interface | Existing movie playback, audio sync and skip behavior; no Windows import library in native targets |
| CPU | Portable scalar reference functions, then architecture-specific acceleration where measured | SSE replacements preserve texture, animation and numerical behavior on ARM64 |

[SDL3](https://wiki.libsdl.org/SDL3/FrontPage) supports Windows, Linux and macOS.
[bgfx](https://bkaradzic.github.io/bgfx/overview.html) supports Vulkan and Metal;
its integration and shader conversion are still project work. If the graphics
prototype fails, record the specific gap and compare a direct Vulkan/Metal adapter
or SDL GPU before selecting an alternative. Avoid maintaining multiple new full
renderers during the initial port.

The inspected Apple ports above make direct Metal and their shader strategies
concrete reuse candidates. bgfx remains a candidate, not a prerequisite to original
client startup or early macOS integration. Keep implementation selection explicit
and supported by representative behaviour on both target platforms.

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

The release acceptance path is M0 -> M1 -> M2 -> M3 -> M4 -> M5 -> M6 -> M8.
Implementation may overlap: the current integration direction prioritises original
client startup and permits early transport, graphics-reference and macOS work
without waiting for every preceding component or release check to finish.
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
Apply the [current integration direction](#current-integration-direction--2026-10-25)
for new work. The reports below retain historical source/evidence and next-caller
descriptions; they do not require a separate widget qualification before integrated
original-client startup can be attempted.

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

1. Bring up integrated original-client startup and the complete signed original
   login UI through the existing SDL3/Gx services. Reuse original Frame/Glue
   source groups and add required widgets/services in coherent batches. Adapt
   remaining legacy OpenGL/window dispatch as needed; keep compatibility-profile
   requirements explicit and report missing capabilities with actionable errors.
2. Expand graphics coverage as original login/world callers become reachable.
   Validate shader loading, UI/text, models, terrain, world objects, particles,
   shadows and water against M0 captures. Check clipping/depth, blending, gamma,
   texture formats and render-target transitions.
3. Adapt the active original network dispatch behind existing interfaces, including
   `WowConnectionNet`'s WSA event/worker dispatcher. Determine reachable Os IOCP
   requirements from the integrated startup/session path. Preserve connection
   lifetimes, callback threads/order, framing, partial reads/writes, cancellation,
   error reporting and disconnect/reconnect behavior. Leave 12340 packet formats,
   authentication and serialization semantics intact.
   The [native packet prerequisite target](docs/m3/PACKETS.md)
   is qualified separately in PR #25; it does not establish transport or login.
4. Complete interactive login, character selection, world entry and movement
   against GrimGuzzler with its unchanged policy and a current identity record.
   Historical M0 server evidence remains separate. Test zoning/teleports, combat,
   chat, inventory and logout, and record actual Warden outcomes.

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

The [SDL IME adapter](docs/m3/FRAME_IME.md) in
[PR #55](https://github.com/RovxBot/wowdecomp/pull/55) is qualified at
`77177a99bcd2ef524c42bfb9bdeba1081cd07f61`, merged as
`da722de8093aba62baa59805c11e35c7e6b857b9`. Its [record](docs/m3/frame-ime-validation.json)
retains thirteen passing PR workflows, 37 CTest checks per Linux GL configuration,
signature-enabled full Windows x64 compilation at matching-tree checkout
`5629a4093141fe46d9e140c4793620bac0066828`, three verified archives and all 51 packaged
executable hashes/types. Candidate-only focus cancellation also passed on the
SDL-only actual ARM64 macOS, Windows and three Linux configurations.

Downloaded RX 7900 XTX release/strict ASan/UBSan/leak packages passed 26 authored/
original-data IME, keyboard/window/hierarchy and prior-resource executions, three
cycles each, with empty stderr/zero workers; all four resource fixtures passed
three cycles per mode. Captures match the prior baseline and each other. Native
composition/candidate/result events now use original stratum/priority CHAR queues,
typed consumers and event ID with explicit UTF-8 code page 65001. Deliver immediately
after OsQueueGet, before another dequeue replaces the borrowed OsIME snapshot;
consumers must not pump/drain the queue. Raw actions/flags are bounded before
callbacks, and traversal guards permit deferred deletion while rejecting mutation,
destruction and reentry. Focus loss cancels candidate-only sessions. Native
language-mode action zero remains unsupported by SDL. This is injected input,
not physical IME/language acceptance or actual edit-box/candidate UI.

Pooled Lua/XML/language/template factories, mouse/occlusion/drag, other Frame
consumers, full signed ordered GlueXML plus post-creation digest gate, actual login
rendering and transport/gameplay remain open. No new physical-input, Windows
runtime, macOS graphics or server acceptance. M3 remains incomplete; preserve
signatures, server revision discrepancies and interactive user sign-in.

The [bounded SDL pointer adapter](docs/m3/FRAME_MOUSE.md) in
[PR #58](https://github.com/RovxBot/wowdecomp/pull/58) is qualified at
`205d3d5770c3ca756909f9c0de63c7a0f3a294a3`, merged as
`53c259f20cb48db3e5e6a4011074b5d0644ad447`. Its [record](docs/m3/frame-mouse-validation.json)
retains thirteen passing PR workflows, 38 CTest checks per Linux release/strict
ASan/UBSan/leak configuration, signature-enabled full Windows x64 compilation at
matching-tree checkout `51970b1b468c9a5f5310a56d742895fe913bf414`, three verified
archives and all 51 packaged binary hashes/types/notices. Downloaded RX 7900 XTX /
Mesa 26.2.5 / SDL X11 packages passed 30 authored/original-data input/UI/resource
executions, three owner cycles each, with empty stderr/zero workers; each of four
resource fixtures passed three cycles per mode. All fifteen release/sanitizer
capture pairs match, including the prior original-data hierarchy/input baseline.

Original queues now deliver hit/inset tracking, enter/leave, captured multi-button
release, wheel, relative callbacks and typed C++ drag/drop consumers. Preserve
logical-pixel/inverted-Y conversion, original raw-relative-to-DDC behavior, button/
modifier masks/event identities/timestamps, raw unsigned/finite preflight, owner/
current-context and traversal guards. Stationary focus refresh follows update
traversal and priority/inset changes; cancellation finishes exit cleanup even when
a release consumer throws. Defer deletion until traversal ends and retire focus,
capture and drag state on hide/disable/destruction/loss. Empty base Lua handlers
are selected; nonempty handlers remain unavailable before stack mutation.

Top-level occlusion/raising, title/layout movement and complete widget drag remain
open. Pooled Lua/XML/language/template factories, actual edit boxes and physical
IME/mouse acceptance, other Frame consumers, full signed ordered GlueXML plus its
post-creation digest gate, actual login and transport/gameplay remain open. The
source-graph/local fixture corrections and superseded attempts remain recorded;
no detector was disabled. No new physical-input, Windows runtime, macOS graphics
or server acceptance. M3 stays incomplete; preserve signatures, server revision
discrepancies and interactive user sign-in.

The [original top-level Frame package](docs/m3/FRAME_STACKING.md) in
[PR #60](https://github.com/RovxBot/wowdecomp/pull/60) is qualified at
`1d675153d9fb6a18f70feb46e5808ceec2749138`, merged as
`2e81a1978a5dc34b82db5ce6457b69950199a415`. The [durable record](docs/m3/frame-stacking-validation.json)
retains all four triggered implementation PR workflows: source audit, Linux GL,
CPU layout matrix and full signature-enabled Windows x64 compilation. Both Linux
GL configurations passed 38 checks; CPU layout also executed on actual ARM64 macOS
and Windows. Hosted checkout `d45d48d3efe6a995cbfdc19e6f1bc154bcb795ee`
has the same source tree as the selected implementation and merge. Three archive
digests, all 51 packaged executable hashes/types and notices verified.

Downloaded RX 7900 XTX / Mesa 26.2.5 / SDL X11 release and strict ASan/UBSan/leak
packages passed 30 input/UI/resource executions with three owner cycles each,
empty stderr and zero workers. Each target/transform/buffer/texture fixture passed
three cycles per mode. All fifteen release/sanitizer capture pairs match; final
original-data hierarchy/input captures retain the prior baseline. The inspected
640x480 original-data mouse capture is authored white text with original Friz
Quadrata/UI shaders, not original login rendering. Intermediate stacking buffers
assert red/green draw-order changes, repeat pixels and restoration; those buffers
are not exported captures.

Original same-stratum positive-rectangle occlusion, descendant exclusion, nearest
top-level ancestor raising, hidden registered level preservation and literal sparse
compression now run in the native adapter. Preflight final raised descendants
before list mutation. Retire dirty batches across every allocated level, including
old source levels above the compressed active range. Keep callback guards on both
sides of internal mouse-down raising and retain resize-callback guards. The first
hosted release failure (37/38, duplicate text-batch ownership after compression)
and superseded runs remain recorded; no ownership/pixel assertion or detector was
disabled. Windows-selected source tokens match the immutable pre-PR base.

Title/layout move/resize, pooled Lua/XML/language/template factories, complete
widget behavior and physical mouse/IME/edit-box/candidate acceptance remain open.
Full signed ordered GlueXML creation and its post-creation digest gate, actual
login rendering, transport/authentication and observed gameplay remain open.
No new physical-input, Windows runtime, macOS graphics or server acceptance.
M3 remains incomplete; preserve signatures, historical identities, server revision
discrepancies and interactive user sign-in.


The [original Frame movement package](docs/m3/FRAME_MOVEMENT.md) in
[PR #62](https://github.com/RovxBot/wowdecomp/pull/62) is qualified at
`e0540e9b194d71b26459b1d78c0832b910688de4`, merged as
`3b9f9f820905e04ce38a18f91661acbd6e4de7db`. [Durable validation](docs/m3/frame-movement-validation.json)
retains all thirteen passing implementation PR workflows, 38 CTest checks per
Linux release/strict ASan/UBSan/leak configuration and full signature-enabled
Windows x64 compilation at matching-tree checkout
`534d8eb6e229ba3f63fdb4aae0a19df85ce8765c`. Three archives/all 51 packaged binary
hashes/types/notices verified. Exact-source RX 7900 XTX / Mesa 26.2.5 / SDL X11
release and strict sanitizer packages passed 30 input/UI/resource executions,
three cycles each, with empty stderr/zero workers; all fifteen final capture pairs
match. Original signatures and the qualified GlueFonts subset remain enabled.

Original title-region hit movement, Alt layout moves, Control edge/corner resizing,
explicit C++ script-reason movement, nine-point DragBy, effective scale, resize
bounds and flatten/unflatten now use the native root. Title regions are direct
C++ parent-owned allocations, not pooled Lua/XML factories. Stop retires the
borrowed movement pointer before unflatten callbacks. Hide/disable/destruction,
host focus/capture loss and deferred deletion retire movement. Preserve guarded
callbacks and defer raising until resize traversal finishes. Attached-text movement
and exact repeats are asserted in intermediate framebuffer readbacks; final captures
remain authored hierarchy text, not original login rendering. No failed hosted/
runtime attempt or detector suppression was needed; duplicate push runs remain
separate from the thirteen passing PR qualification runs.

Pooled Lua/XML/language/template factories, remaining widgets, physical mouse/IME/
edit-box/candidate UI acceptance, complete signed ordered GlueXML creation and its
full post-creation digest gate, actual login rendering, transport/authentication
and observed gameplay remain open. No new physical-input, Windows runtime,
macOS graphics or server acceptance. M3 stays incomplete; preserve signatures,
historical identities, server revision discrepancies and interactive user sign-in.


The [pooled Frame allocation package](docs/m3/FRAME_POOLS.md) in
[PR #64](https://github.com/RovxBot/wowdecomp/pull/64) is qualified at
`d2088a7cab528ca915b5fb1faa3cb354048ebd0a`, merged as
`f3e74f19562583ded6c8aeb414f4dccf60782da0`. [Durable validation](docs/m3/frame-pool-validation.json)
retains thirteen passing PR workflows, 38 CTest checks per Linux release/strict
ASan/UBSan/leak configuration and full signature-enabled Windows x64 compilation
at matching-tree checkout `eb7985d3665dcbd81f9c1c40b9be4543938f15ab`. Three archives,
all 51 executable hashes/types and notices verified. RX 7900 XTX / Mesa 26.2.5 /
SDL X11 packages passed 30 three-cycle input/UI/resource executions, empty stderr
and zero workers; all fifteen capture pairs match. Four resource fixtures also
passed three cycles per mode. The initial restricted X11-unavailable launch remains
failed evidence; the same binary passed with desktop access and unchanged detectors.

Original `Create_SimpleFrame` and a typed native FontString constructor now use
original class pools. Preserve native slot alignment, exception rollback, mixed
heap/pool deletion and parent/root ownership. Pools register with the Fonts owner;
close all consumers/raw slots before it clears blocks. Growth to 101/810 slots,
reuse, throwing construction, recursive/deferred/root destruction, actual pooled
text rendering/repeat pixels and owner release are qualified. This does not install
Lua/XML registry/metatables, templates/language or remaining widget factories.
Route each selected allocator/deletion before enabling those callers. Original
signatures and before/after GlueFonts-subset interface checks stay enabled.

Region/frame/title/FontString metatables, named-parent/template/language processing,
nonempty Lua input handlers, remaining widgets and physical mouse/IME/edit-box/
candidate UI acceptance remain open. Complete signed ordered GlueXML and its full
post-creation digest gate, actual login rendering, transport/authentication and
observed gameplay remain open. No new physical-input, Windows runtime, macOS
graphics or server acceptance. M3 is incomplete; preserve historical identities,
signatures, server revision discrepancies and interactive user sign-in.


The [bounded original region Lua package](docs/m3/FRAME_SCRIPTS.md) in
[PR #66](https://github.com/RovxBot/wowdecomp/pull/66) is qualified at
`56aa592edfc7685c7234119aab874f55173bf61f`, merged as
`914a6d5b72a3edee185379559c5b56a2ebd9b273`. [Durable validation](docs/m3/frame-script-method-validation.json)
retains thirteen passing PR workflows, 38 CTest checks per Linux release/strict
ASan/UBSan/leak configuration and full signature-enabled Windows x64 compilation
at matching-tree hosted checkout `3c9d0ba7edc0a4bc9004907a3cdafe1919e18a62`.
Three archives/all 51 executable hashes/types/notices verified. RX 7900 XTX / Mesa
26.2.5 / SDL X11 packages passed 30 three-cycle authored/original-data desktop
executions, empty stderr/zero workers; all fifteen capture pairs match. Four
resource fixtures also passed three cycles per mode. All compilation ran in Actions.

Original region/Frame/title/FontString metatables now expose bounded identity,
parent/name, dimensions/anchors, hierarchy enumeration, visibility, title reuse and
pooled Lua `CreateFontString` with named-font inheritance. Preserve canonical live
Lua tables, finite coordinates, checked point indices and owner/current-context/
live-root/traversal admission. C++ failures unwind before Lua errors. Close every
region/script object before the Scripts owner, then renderer/Fonts/Gxu/layout/XML/
VM/device/context/queue. Original draw-layer converters moved unchanged to shared
FontXML helpers; Windows-selected legacy tokens match the immutable base.

Language/text setters and XML virtual templates remain explicit Lua errors. The
new rendering fixture uses authored C++ `process=0` text in Lua-created objects;
intermediate rendering/repeats are readback assertions, not exported new captures
or login composition. Original signatures and before/after GlueFonts-subset checks
stay enabled. A usage-limit approval-review failure interrupted one unexecuted
sanitizer attempt; resumed strict checks passed with unchanged detectors.

Original FrameXML registry/global `CreateFrame`, named-parent XML processing,
templates/inheritance/language, additional methods/nonempty Lua input handlers,
remaining widgets and physical mouse/IME/edit-box/candidate UI acceptance stay
open. Complete signed ordered GlueXML and its full post-creation digest gate,
actual login rendering, transport/authentication and observed gameplay remain open.
No new Windows runtime, macOS graphics or server acceptance. M3 remains incomplete;
preserve historical identities, signatures, server revision discrepancies and
interactive user sign-in.


The [original language/text-setter package](docs/m3/LANGUAGE.md) in
[PR #68](https://github.com/RovxBot/wowdecomp/pull/68) is qualified at
`f0f522c467177cb1c044758c0f686d3001e9f3a4`, merged as
`2a0e3f5105fcb81064d287ba515fec2639c20151`. [Durable validation](docs/m3/language-validation.json)
records thirteen passing implementation workflows, 38 CTest checks per Linux
release/strict ASan/UBSan/leak configuration and full signature-enabled Windows
x64 compilation at matching-tree checkout `fb6ba2fe87c3129ca078a8182c8ff952777c9ebf`.
Three archives/all 51 executable hashes/types/notices verified. RX 7900 XTX / Mesa
26.2.5 / SDL X11 release/strict sanitizer packages passed 30 three-cycle authored/
original-data desktop executions, empty stderr/zero workers; fifteen capture pairs
match. Four resource fixtures passed three cycles per mode.

Original Korean particle/rieul, French article/vowel and plural decisions now back
Lua `SetText`/positional `SetFormattedText`. Text numeric conversions are separate
from coordinate finite-float checks. Preserve 4095-byte valid UTF-8 intermediate
admission, grammar/article nouns, signed-32-bit counters, 64 passes, owner-thread
borrowed scratch buffers and C++ unwind before Lua errors. Region/direct-C++ and
Lua fixtures assert processing, unchanged text on rejection, retained caches,
rendered transformed authored text, exact repeats and teardown. Windows language/
renderer/setter tokens remain unchanged. The prior link and stale-fixture failures
remain evidence; detectors stay enabled. Original-data signatures stay enabled.

DB-backed declensions, XML registry/global `CreateFrame`/named-parent/templates,
remaining widget/nonempty Lua input consumers and physical IME/edit-box/candidate
acceptance remain open. Complete signed ordered GlueXML/full second digest gate,
full login rendering, transport/authentication and observed gameplay remain open.
No new physical-input, Windows runtime, macOS graphics or server acceptance. M3 is
incomplete; preserve historical identities, signatures, server revision discrepancies
and interactive user sign-in.


The [original base Frame XML package](docs/m3/FRAME_XML.md) in
[PR #70](https://github.com/RovxBot/wowdecomp/pull/70) is qualified at
`2932c2fdb7eac99bc1a583f3c4374e7ab49fc71c`, merged as
`70faf33a85e5a967530ea463d7da4f9d94fc7ba1`. [Durable validation](docs/m3/frame-xml-validation.json)
records thirteen passing final-head PR workflows, 38 CTest checks per Linux
release/strict ASan/UBSan/leak configuration and full signature-enabled Windows
x64 compilation at matching-tree checkout `da814becf9a5a35681062ad14263e82cc07820c1`.
Three archives/all 51 packaged executable hashes/types/notices verified. RX 7900
XTX / Mesa 26.2.5 / SDL X11 release/strict sanitizer packages passed 30 three-cycle
authored/original-data executions, empty stderr/zero workers; all fifteen capture
pairs match. Implementation, hosted checkout and merge share one source tree.

Original base Frame registry, Lua `CreateFrame` and FontString XML-template
consumers now use the qualified pools/hierarchy/rendering. Preserve transferred
nodes beyond XML tree close, original Frame sibling-parent precedence, exact
FontString inheritance keys and class-specific taint semantics. Exceptions unwind
locks/taint and allocated descendants, including explicit external parents.
Global name/parentKey assignments reject Lua assignment-hook reentry; template
and numeric/name admission stays bounded. Close consumers before Scripts and its
nodes/registry storage, then renderer/Fonts/Gxu/layout/XML/VM/device/context/queue.
Original Windows-selected code and signature defaults stay intact. The initial
no-ink fixture and FreeType empty-outline UBSan failure remain evidence; native-only
empty-copy guards have a focused regression and no detector was disabled.

This is authored XML/text composition with original or authored resources, not
full login layout. Texture/backdrop/animation/model and remaining widget factories,
actual nonempty Lua handlers, broader XML/template semantics, DB-backed declensions
and physical IME/edit-box/candidate acceptance remain open. Complete signed ordered
GlueXML/full post-creation digest gate, full original login rendering, portable
transport/authentication and observed gameplay remain open. No new physical-input,
Windows runtime, macOS graphics or server acceptance. M3 remains incomplete;
preserve signatures, historical identities, server revision discrepancies and
interactive user sign-in. The clean client installation remains read-only.


The [original Frame texture package](docs/m3/FRAME_TEXTURES.md) in
[PR #72](https://github.com/RovxBot/wowdecomp/pull/72) is qualified at
`6727ee12de9dd44a74dd548b63764efe35da0244`, merged as `519be5f497074a582419f0a9276e73d82ff69c3a`.
[Durable validation](docs/m3/frame-texture-validation.json) records thirteen passing
final-head PR workflows, 38 CTest checks per Linux release/strict ASan/UBSan/leak
configuration and full signature-enabled Windows x64 compilation at matching-tree
checkout `a8c63b738b0b45081f7c71bb77b9e3182b6d9986`. Three archives/all 51 packaged binary
hashes/types/notices verified. Implementation, hosted checkout and merge share one
source tree. After the interruption, fresh RX 7900 XTX / Mesa 26.2.5 / SDL X11
release/strict packages passed 30 three-cycle authored/original-data desktop
executions on 2026-10-24, with empty stderr and zero workers. Four resource fixtures
passed three cycles per mode; all fifteen final capture pairs match the prior baseline.

Original CSimpleTexture regions now use native-aligned original pools, canonical
Lua CreateTexture and exact-key retained XML templates/layers. Selected image,
visibility/color/gradient/alpha/coordinate/rotation/blend/draw-layer/desaturation
methods use existing Services/Gx and retained quads. Preserve native flag
initialization, raw image-mode/sampler admission, dirty coordinate/nonblocking
updates, retained image/shader handles and GPU-before-solid-pixel teardown.
Original 8x8 ARGB solid extents/opacity are preserved without the global solid cache.
The fixture covers growth to 737 slots, authored atlas coordinates, original
UI/Desaturate shaders and original loading-bar pixels, plus XML/name/parentKey/
stale-table and repeat/replacement/visibility/alpha lifetimes.

Close objects before Scripts/metatables/image-mode shaders, then image Services,
renderer/Fonts/Gxu/layout/XML/VM/device/context/queue. Animation and tiling mutation
remain unavailable. Windows-selected texture/Frame/renderer bodies and extracted
XML helpers retain original tokens. The missing-declaration native compiler failure
and Windows orientation-table extraction failure remain recorded; no detector or
pixel assertion was disabled. All compilation ran in Actions. Original-key archives,
selective MD5 and before/after GlueFonts-subset InterfaceKey checks remain enabled.

Composition is authored, separate from full login. Intermediate GPU readbacks assert
texture pixels; final captures remain the historical authored hierarchy text.
Remaining button/edit-box/backdrop/animation/model/scroll widgets, nonempty Lua
handlers, complete signed ordered GlueXML/full second digest gate, actual login,
transport/authentication and observed gameplay remain open. Physical IME/edit-box/
candidate acceptance remains pending. No new Windows runtime, macOS graphics or
server acceptance; preserve server revision discrepancies, interactive sign-in and
read-only clean data. M3 remains incomplete.

The [original Frame Lua/XML handler package](docs/m3/FRAME_HANDLERS.md) in
[PR #74](https://github.com/RovxBot/wowdecomp/pull/74) is qualified at
`9d3384f9436aaa9c763fca0c35a8a91b1be734b9`, merged as
`1cea05886d4fa4b68552f1615030e18ce7de111b`. Its [record](docs/m3/frame-handler-validation.json)
keeps final-head 38/38 Linux GL release/strict sanitizer checks separate from
12 prior passing production/non-GL workflows at `ac2b05f84ab00920244b987096ed59d923fd8e9e`.
Full Windows x64 compiled with signatures at `a2e991897220e6ad51bea9193cc85866a7c06929`;
actual ARM64 prerequisites passed. Only two GL fixtures/scheduling docs changed
later; all other 3,876 tracked files match. Three archives/all 51 executable
hashes/types/notices verified. Downloaded RX 7900 XTX release/strict sanitizer
packages passed 30 three-cycle authored/original-data executions, empty stderr/
zero workers; fifteen capture pairs retain the prior baseline. Callback readbacks
require distinct A-to-V-to-AV pixels, separate from original login composition.

Original base Frame script/event methods, XML wrappers/caches, nonempty original
root/Frame callbacks and bounded text updates now use the qualified owners.
Preserve registry/function lifetimes, taint/hook/listener semantics, raw internal
globals and callback structural guards. Prior fixture and Actions-disable failures
remain evidence. [CI scheduling](docs/CI_BUILD_POLICY.md) removes duplicate branch
push runs and documents exact-input reuse for fixture-only corrections. Remaining
widgets/broader callback edits, physical IME/edit-box acceptance, full signed
GlueXML/second digest gate, actual login and transport/gameplay remain open.
No new operator, Windows runtime, macOS graphics or server acceptance is claimed.
M3 is incomplete; signatures/server discrepancies/interactive sign-in stay intact.


The original [Button package](docs/m3/BUTTONS.md), [PR #78](https://github.com/RovxBot/wowdecomp/pull/78),
is qualified at `afb952dc28ce9772b1b5edfa1f19bb4ed2b59e4e`. Final-source Linux GL
release/strict ASan/UBSan/leak checks passed 38 CTest checks each. Earlier production
`defccaa8914e737a93df7585b3d556a124c7ec07` passed all four native asset configurations
(eight checks each, including actual ARM64 execution) and full signature-enabled
Windows x64 compilation; only a GL fixture/document changed afterward, with 3,889
other entries identical. Seven archives/all 67 packaged executable hashes/types
verified. Sixteen three-cycle RX 7900 XTX / Mesa 26.2.5 / SDL X11 wrapper executions
passed with empty stderr/zero workers. Eight final capture pairs, five original
button pairs and eight historical regression comparisons match. Original
GlueFontStyles declarations/inline colors and GlueButtonTemplateBlue render an
authored label, with before/after InterfaceKey checks and original MPQ policy.
Nonmip UI reads validate bounded original thin DXT tail storage without decoding
or inventing its pixels; full mip reads retain strict rejection. Preserve failed
attempts and distinguish diagnostic GDB/source inspection from qualification.
CheckButton/EditBox, broader callbacks, complete signed ordered GlueXML/full second
digest gate, actual login, transport/authentication and gameplay remain open. No
new physical-input, Windows runtime, macOS graphics or server acceptance; clean
data stays read-only and user sign-in stays interactive. M3 remains incomplete.

### M4 — Complete the Linux experience and select modern graphics

Depends on: M3 for Linux completion. Graphics reference review/prototypes and
early macOS integration may proceed during M3; backend selection must precede a
full maintained-renderer migration.

1. Implement and validate audio device output, decoding/streaming, positional sound,
   music, volume controls and device changes. Document capture/voice support
   separately according to server capability; silent placeholders do not count.
2. Port movie playback and portable crash diagnostics. Validate repeated startup,
   world transitions, device/resource recreation and clean shutdown.
3. Test SDL integration under X11 and Wayland, including fullscreen/windowed mode,
   mouse capture, alt-tab, DPI and multi-monitor behavior on available hardware.
4. Compare reusable native Metal Gx/shader implementations with the proposed
   Gx/bgfx route before selecting further renderer work. Prototype UI/text, terrain,
   a skinned character, representative shader permutations, compressed textures,
   depth and offscreen rendering. Exercise the proposed Linux Vulkan path and
   obtain an early Metal hardware smoke test so selection addresses both targets.
5. Record the backend/dependency decision, shader strategy, minimum OS/GPU/toolchain
   requirements and remaining unsupported features. Keep the existing renderer as
   a selectable comparison path while the replacement matures.

Exit: Linux has an end-to-end playable baseline including sound, settings and
movies, and the maintained graphics direction has demonstrated representative
behavior on Vulkan and Metal. Report driver/session coverage actually tested.

### M5 — Implement the maintained renderer and ARM64 core support

Depends on: M4 graphics decision. CPU isolation and scalar implementations can
start earlier without waiting for graphics completion. Reviewed Metal adaptation
and integrated Apple startup may also proceed before this release milestone.

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

Depends on: M5 for release completion. macOS platform/renderer adaptation and
integrated original-client startup can begin during M2-M4 as dependencies and
hardware permit; do not defer them until all Linux widgets are qualified.

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

Apply this contract to the changed interfaces and integration path. Keep required
workflow checks and regressions, and batch additional validation around coherent
changes. Existing probes protect qualified behaviour; exhaustive standalone
qualification of all modules/widgets is not a prerequisite to exercising the
integrated client. Record actual failures and add focused regressions without
disabling detectors or weakening original-data/signature/server requirements.

## Current implementation priorities and completed foundations

Use the current integration deliverables above for new work; maintain milestone
status as evidence arrives. Completed foundations and historical packages below
remain evidence, rather than a requirement to continue one widget at a time.

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
- [ ] Add an Actions-built integrated Linux original-client startup target through
      the existing SDL3/Gx/native services and resolve its reachable dependencies.
- [ ] Render and interact with the complete signed original login UI, including
      ordered GlueXML execution/full post-creation verification and clean shutdown.
- [ ] Adapt active original transport/authentication and observe interactive
      GrimGuzzler character selection/world entry with unchanged server policy.
- [ ] Audit the pinned Apple ports for reusable platform/network/Metal code and
      begin macOS integration as dependencies and hardware permit.
- [ ] Complete sound/movie support and select the maintained Linux/macOS graphics
      direction using representative prototypes and reviewed reuse candidates.

The original [CheckButton package](docs/m3/CHECKBOXES.md),
[PR #79](https://github.com/RovxBot/wowdecomp/pull/79), is qualified at
`2f0035fef2a64bbb6fe3c6fefbeb2ad9c099b198`. Both final-source Linux GL configurations passed
38 CTest checks; full signatures-enabled Windows x64 compilation passed on that
same implementation. Initial production passed twelve other PR workflows,
including actual ARM64 execution; the initial
GL compile failed on fixture event names and remains recorded. The final geometry
fix changed only GL production/fixtures and its contract, with 3,890 other tracked
entries identical to the initial production head. Seven archives/all 67 packaged
binary hashes/types/notices verified.

Downloaded RX 7900 XTX / Mesa 26.2.5 / SDL X11 release and strict ASan/UBSan/leak
packages passed sixteen three-cycle authored/original-data GPU wrapper executions,
with empty stderr and zero workers. Eight final capture pairs, five original
Button pairs and five original CheckButton pairs match; all eight preceding
regression captures also match. Unchanged `GlueCheckButtonTemplate` renders original
checkbox images and original fonts with an authored label, with before/after
InterfaceKey checks and original MPQ/selective-MD5 policy. Read-only inspection
identified original AccountLogin EditBoxes' backdrop/font/script dependencies.

Original-data testing caught a stale retained texture vertex snapshot after
image-dependent layout resolved. Native texture layout now invalidates its layer
for non-tiled geometry as well as tiled UVs; authored region-only move/restore and
exact original unchecked/checked/disabled/re-enabled pixels cover the correction.
Windows-selected bodies and extracted factory match the preceding baseline.
Preserve the earlier compiler/pixel failure, diagnostic-only GDB investigation and
restricted-X11 attempt separately from final untraced sanitizer qualification.
EditBox/backdrop/remaining widgets, complete signed ordered GlueXML/full second
digest gate, login, transport/authentication and gameplay remain open. No new
physical-input, Windows runtime, macOS graphics or server acceptance; clean data
stays read-only, signatures/server discrepancies stay intact and sign-in stays
interactive. M3 remains incomplete.

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

## Original tiled backdrop qualification — 2026-10-24

The original [backdrop package](docs/m3/BACKDROPS.md),
[PR #80](https://github.com/RovxBot/wowdecomp/pull/80), is qualified at
`5b0c26a9a8f5da7044bbf9352235bb7b411d3a76`. [Durable validation](docs/m3/backdrop-validation.json)
records final-source Linux GL release/strict ASan/UBSan/leak configurations passing
38 CTest checks each, plus four native asset configurations passing eight checks
each, including actual ARM64 macOS execution. Full signature-enabled Windows x64
compilation passed at `05aad12dd224256928192968233863501e078645`; later edits are
native-only guards/Portable code and fixtures/audits. Windows-selected generator/
Frame tokens remain unchanged. Preserve these separate compilation identities.
Seven archives/all 67 packaged executable hashes/types/notices verified.

Downloaded RX 7900 XTX / Mesa 26.2.5 / SDL X11 release and strict sanitizer packages
passed twelve three-cycle authored/original-data wrapper executions with empty
stderr and zero workers. All 21 final capture pairs match; twelve historical
capture comparisons match the preceding CheckButton qualification. The unchanged
AccountLogin EditBox backdrop node and field size render original tooltip images
on an authored Frame/placement, with before/after subset InterfaceKey checks and
original signaturefile/selective-MD5 policy. This is an empty field backdrop, not
EditBox text entry, complete AccountLogin or its full post-creation digest gate.

Nine-piece generators own aligned pooled texture regions through Frame lifetime.
Replace outside traversal/collection; transfer the generator only on success.
Failed output construction returns temporary regions and preserves prior output.
Close Frame regions before their generator, then Scripts/images/renderer/Fonts/
layout/XML/VM/device/context/queue. Preserve original side/corner UVs/insets and
Windows bodies. Native tiled background sampling repeats both axes. General
texture tiling mutators remain unavailable. Original tooltip BLPs exposed a
single-block unrequested 16x2 DXT tail; the bounded base-only reader admits that
shape while full-chain reads still reject missing pixels. No tail pixels are
decoded, invented or uploaded. DXT1/3/5 authored regressions cover both axes and
malformed/truncated/nonthin/unflagged storage. Retain the initial source-allowlist
failure, original-data rejection and diagnostic-only GDB header attempts.

EditBoxes/fonted-frame storage and IME/candidate ownership, remaining widgets,
complete signed ordered GlueXML/full second digest gate, actual login,
transport/authentication and observed gameplay remain open. No new physical-input,
Windows runtime, macOS graphics or server acceptance. Clean data stays read-only,
signatures/server revision discrepancies stay intact and sign-in stays interactive.
M3 remains incomplete.

## Original MessageFrame qualification — 2026-10-24

The original [MessageFrame package](docs/m3/MESSAGE_FRAMES.md),
[PR #81](https://github.com/RovxBot/wowdecomp/pull/81), is qualified at
`9a8ffc528320d0ca12fa8f1b4e7120022b4ab62c`. [Durable validation](docs/m3/message-frame-validation.json)
records both final-source Linux GL release/strict ASan/UBSan/leak configurations
passing 38 CTest checks each. Full signatures-enabled Windows x64 compilation
passed at `9e034144c95186a60f93e292e44900183454e8c1`; later production edits are
native-only row transactions and later fixtures/docs. Windows-selected widget/root
bodies and extracted private-font/factory bodies match the preceding baseline.
Preserve these separate build identities. Three archives/all 51 packaged binary
hashes/types and notices verified; no client/dependency compiled locally.

Downloaded RX 7900 XTX / Mesa 26.2.5 / SDL X11 release and strict sanitizer packages
passed twelve three-cycle authored/original-data executions with empty stderr and
zero workers. All 27 release/sanitizer capture pairs match; all 21 preceding
backdrop-package captures match. Message text/candidate-like content and widget XML
are authored; original-data mode uses original Friz/UI shaders and existing signed
GlueFonts/Button/CheckButton/backdrop subsets. Actual EditBox/candidate integration
and complete signed ordered GlueXML/full second digest are not established.

Private fonts and reference-counted row nodes use matching native allocation;
pooled MessageFrame/FontString slots retain alignment and close through the Fonts
owner. Resize/font callbacks only invalidate rows; prepare ownership before root
traversal. Grow temporary row owners before publication and return all of them on
admission/allocation failure. Close rows/text blocks before private fonts, then
widgets before Scripts/images/renderer/Fonts/Gxu/layout/XML/VM/device/context/queue.
Preserve original pending copies, insertion, offsets, rounding epsilon and fading.
Initial native bounds: 4095-byte UTF-8 messages, 1024 pending messages, 256 rows.
Keep the initial source-allowlist failure, pressure-fixture layout-order failure,
original-font row-stride expectation failure and superseded jobs as evidence;
no detector was disabled.

EditBox text/focus/region ownership and retained candidate delivery are next,
followed by remaining widgets, complete signed GlueXML, portable transport/auth and
observed login/gameplay. No new physical-input, Windows runtime, macOS graphics or
server acceptance. Clean data stays read-only, signatures/server discrepancies
stay intact and sign-in stays interactive. M3 remains incomplete.

## Original EditBox qualification — 2026-10-25

The original [EditBox package](docs/m3/EDITBOXES.md),
[PR #82](https://github.com/RovxBot/wowdecomp/pull/82), is qualified at
`50b7af672c4f2f5440e94476ca1e658e5c34358f`. [Durable validation](docs/m3/editbox-validation.json)
retains final-source Linux GL release/strict ASan/UBSan/leak checks and the separate
signature-enabled full Windows x64 compilation checkout
`0f728d90a8c18fc3c19d2fcfc55ffbe81177c593` from initial production
`e1dca531d9c516eeb19b88d6a817a6903b816385`. Twelve initial non-GL workflows passed,
including actual ARM64 prerequisites; later edits change native-only EditBox code,
a GL fixture and its contract. The other 3,902 tracked entries match. Preserve
those older check identities; they are not final-head or macOS EditBox runs.

Three archives and all 51 packaged binary hashes/types/notices were verified.
Final-source RX 7900 XTX / Mesa 26.2.5 / SDL X11 release and strict sanitizer
packages passed twelve three-cycle authored/original-data desktop wrappers, with
empty stderr and zero workers. All 38 capture pairs match, including the original
field and candidate rows; all 27 preceding MessageFrame captures remain unchanged.

Original single-line CSimpleEditBox now has pooled Lua/XML creation, private font/
FontString/cursor/selection ownership, UTF-8 editing/limits, clipboard, password/
numeric/history handling, original key shortcuts and retained IME candidates.
Keep one SDL text-input request per focus, including password/numeric fields;
transfer/hide/window focus loss/destruction balance it. Guard recursive focus and
hold callback lifetimes. Enter callback state unwinds on failure. Native deletion
uses overlapping-safe moves, aliased setters retain source bytes and explicit
cursor/selection offsets respect byte boundaries. Multiline, OS secure input and
native language selection remain unavailable.

Retain snapshots at dequeue. Construct/show/hide candidate Frames and prepare rows
outside input/resize/update traversal; anchor resolution may require another root
update. Close candidates and text consumers before their private font, then widgets
before Scripts/images/renderer/Fonts/Gxu/layout/XML/VM/device/context/queue. Preserve
structural guards and original Windows paths. Candidate pages are bounded to 32,
text to 4095 UTF-8 bytes and history to 128. Clean data stays read-only.

Original-data mode constructs the AccountLogin field size/font/backdrop/inset subset
with authored placement and typed sample text; original scripts/anchors are omitted
explicitly. Before/after subset InterfaceKey and original MPQ/selective-MD5 checks
remain enabled. This is field testing, separate from complete signed ordered
GlueXML/full second digest, actual login/authentication and gameplay. Keep failed
hosted visibility-guard and premature candidate-row expectations, the restricted-X11
attempt and prior successful-source reports separately. No detector was disabled.
No new physical-input, Windows runtime, macOS graphics or server acceptance.
Remaining widgets, full GlueXML and portable transport/authentication still gate
interactive user sign-in and observed world entry. Preserve server revision
discrepancies and historical qualification. M3 remains incomplete.


## Original StatusBar qualification — 2026-10-25

The original [StatusBar package](docs/m3/STATUSBARS.md),
[PR #83](https://github.com/RovxBot/wowdecomp/pull/83), is qualified at
`9d6c71c2a1cdae7f2bb1da830c0a6a9d849158eb`. [Durable validation](docs/m3/statusbar-validation.json)
records 38 CTest checks in each final-source Linux GL release/strict ASan/UBSan/leak
configuration. Full signatures-enabled Windows x64 compilation and source audit
passed at production `fd1172c4b938540464202e18a2bdb6d6017c2d70`; only two GL
fixture files changed afterward, with 3,907 tracked entries unchanged. Preserve
these separate identities. Three archives/all 51 packaged binary hashes/types and
notices verified; no local compilation.

RX 7900 XTX / Mesa 26.2.5 / SDL X11 release/strict packages passed twelve
three-cycle authored/original-data wrappers with empty stderr and zero workers.
Additional targeted visible-resize checks passed 36 cycles. All 48 capture pairs
and 24 prior EditBox/widget comparisons match. Pooled native
StatusBar Lua/XML now selects original range/value callbacks, horizontal/vertical/
zero fill, color/rotation and texture regions. Preserve canonical ownership,
structural replacement admission, custom UVs until explicit rotation, finite range/
raw index checks, resize invalidation and widgets-before-owner teardown. The
loading-bar image is original; placement and StatusBar composition are authored.

Retain two earlier visible-mode EditBox preedit/commit failures and thirty passing
targeted repeats. Exact focus-event causes were not captured. Common authored
widget input now runs hidden before the window is shown for SDL resize checks;
production input behavior/assertions/detectors stay unchanged. Keep GDB/ptrace
LeakSanitizer diagnostics separate from final untraced checks. This is no new
physical-input, Windows runtime, macOS graphics or server acceptance. Remaining
widgets/model consumers, complete signed ordered GlueXML/full second digest,
portable transport/authentication and observed login/gameplay keep M3 open. Clean
data stays read-only; preserve signatures, server revision discrepancies and
interactive user sign-in.
