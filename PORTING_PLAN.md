# Native 3.3.5a client implementation plan

Strategy and acceptance criteria; current work and evidence live in the
[issue tracker](https://github.com/RovxBot/wowdecomp/issues). Agent rules are in [AGENTS.md](AGENTS.md).

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
This direction supersedes historical next-caller ordering in old module
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

### Current work and acceptance

Use the [live issue tracker](https://github.com/RovxBot/wowdecomp/issues) for current status, dependencies,
acceptance checklists and execution evidence. The links below are an entry index,
not a second status log. Read issue bodies/comments and linked PRs before choosing
the next step; pending branch results do not establish main behavior.

| Work | Tracking issue |
| --- | --- |
| Original CPU vertex/material dispatch | [#93](https://github.com/RovxBot/wowdecomp/issues/93) |
| Original Texture1 material/multipass continuation | [#94](https://github.com/RovxBot/wowdecomp/issues/94) |
| Protected Lua mutation-guard diagnostic triage | [#95](https://github.com/RovxBot/wowdecomp/issues/95) |
| Complete original login rendering and operator input | [#96](https://github.com/RovxBot/wowdecomp/issues/96) |
| Native DNS/socket dispatch | [#97](https://github.com/RovxBot/wowdecomp/issues/97) |
| Original authentication, live realms and Warden compatibility | [#98](https://github.com/RovxBot/wowdecomp/issues/98) |
| Character screen, enumeration and model lifetimes | [#99](https://github.com/RovxBot/wowdecomp/issues/99) |
| Observed world entry and basic play | [#100](https://github.com/RovxBot/wowdecomp/issues/100) |
| Maintained Linux/macOS renderer selection | [#101](https://github.com/RovxBot/wowdecomp/issues/101) |
| Original macOS ARM64 application/startup | [#102](https://github.com/RovxBot/wowdecomp/issues/102) |
| Native audio / movie playback | [#103](https://github.com/RovxBot/wowdecomp/issues/103), [#104](https://github.com/RovxBot/wowdecomp/issues/104) |
| Applied graphics/gamma/terrain preferences | [#105](https://github.com/RovxBot/wowdecomp/issues/105) |
| Physical EditBox/IME qualification | [#106](https://github.com/RovxBot/wowdecomp/issues/106) |
| Windows runtime baseline and retained OpenGL failures | [#107](https://github.com/RovxBot/wowdecomp/issues/107) |
| Wayland Fontconfig qualification | [#108](https://github.com/RovxBot/wowdecomp/issues/108) |

Progress must name the integrated target, actual execution reached and next
concrete blocker. Preserve original signatures, 12340 behavior, read-only data,
Actions-only compilation, original agreement state and interactive sign-in.
Component fixtures, debugger traces, actual rendering/input and server sessions
remain distinct evidence. Unavailable services fail visibly.

### Inspected Apple integration references

These are additional 12340 implementation references, separate from the historical
M0 product source and pinned 15595 donor. Inspection does not qualify their binaries
on our hardware/data/server or import their code into this fork.

| Reference snapshot | Useful implementation direction | Evidence limits |
| --- | --- | --- |
| [jerrywossion/wowdecomp `apple` at `7489b5f2df6628a434b2ad9aceed66c0f2e6a431`](https://github.com/jerrywossion/wowdecomp/tree/7489b5f2df6628a434b2ad9aceed66c0f2e6a431) | Original client/Glue integration through Apple platform services; direct Metal with recognised original shader families; native transport and focused probes | README reports local original-client/world execution with ASan/UBSan/Metal validation. Documented local login omits executable challenge verification, requires existing `StrictVersionCheck=0` and does not implement native Warden. It does not qualify unchanged GrimGuzzler policy. |
| [fmustafayaman/WoWReMac `WowRe` at `3c7f82c02fa9d4c8399b8f95d30ef0df575ef069`](https://github.com/fmustafayaman/WoWReMac/tree/3c7f82c02fa9d4c8399b8f95d30ef0df575ef069) | Full original client target, bounded Mac compatibility headers/platform replacements and D3D9-bytecode-to-Metal translation | Source and published scene measurements support integrated native rendering work. Warden and Epoch integrations belong to separate branches; their results do not qualify the default branch or our server. |

## Source baselines and evidence

| Role | Pinned reference |
| --- | --- |
| Historical M0 product source | `RovxBot/wowdecomp` at `cd29da38b9162419ee0fa29c6b16e07e10f6c4f0`; preserve the original tutorial bounds correction |
| 4.3.4 reference/donor | [SatyPardus/wowdecomp15595, w15595 at `959b0b85c5f55c565328d7c3ca33027a65e41327`](https://github.com/SatyPardus/wowdecomp15595/tree/959b0b85c5f55c565328d7c3ca33027a65e41327) |
| Later upstream/native imports | [Backport ledger](docs/BACKPORTS.md), [2026-10-18 review](docs/upstream/2026-10-18.md), [2026-10-24 review](docs/upstream/2026-10-24.md) |

Keep reference checkouts outside the product tree and cite exact revisions.
Review source/dependency provenance, notices, ABI, data/shader compatibility and
Windows impact before importing. Do not wholesale merge a donor or introduce
Cataclysm gameplay/network formats into the baseline.

Existing module docs and JSON manifests retain exact historical build/runtime
identities and failed attempts. Repeated per-PR narratives were removed from this
plan during [migration #92](https://github.com/RovxBot/wowdecomp/issues/92); the
[previous plan](https://github.com/RovxBot/wowdecomp/blob/6a18bde50e300fc34d7ed983a2ed1dc97ef76d58/PORTING_PLAN.md) remains available in Git.
New execution evidence belongs in issues, with retained artifacts/manifests as
needed, rather than another plan appendix.

## Architecture decisions

These are implementation defaults and bounded investigations, not claims that the
dependencies or interfaces already exist.

| Area | Direction | Decision evidence required |
| --- | --- | --- |
| Build | GitHub Actions exclusively for client/dependency compilation; CMake, C++17, MSVC on Windows, Clang first on Linux and macOS | Explicit OS/CPU source selection and reproducible dependencies; Windows regression and qualified native targets retain separate evidence |
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

The release acceptance path is M0 → M1 → M2 → M3 → M4 → M5 → M6 → M8.
Implementation may overlap: original startup, transport, graphics review and
macOS integration need not wait for every widget or prior release check. M7
features remain independently scoped and optional.

### M0 — Verify the baseline and map dependencies

Exit: a reproducible Windows runtime baseline or documented pre-existing failure,
named data/server setup and evidence-backed dependency inventory. Source/build
evidence is retained in [M0 baseline](docs/m0/BASELINE.md); remaining runtime and
capture requirements are tracked in [#107](https://github.com/RovxBot/wowdecomp/issues/107).

### M1 — Establish portable build and runtime foundations

Exit: explicit OS/CPU source selection, portable allocation/logging/time/thread
services and a minimal native lifecycle with qualified Windows regression and
native/sanitizer checks. [Foundations](docs/m1/FOUNDATIONS.md) and
[runtime design](docs/m1/RUNTIME_DESIGN.md) define the completed core scope;
component execution does not establish a full native game.

### M2 — Load original assets and create a native application shell

Exit: authenticated original assets, separate writable roots, a usable SDL window,
settings persistence and joined shutdown. [Asset/shell contracts](docs/m2/HANDOFF.md)
link the observed Linux X11 closeout and separate operator/Wayland qualifications;
ARM64 component execution does not establish Cocoa game behavior.

### M3 — Render the login screen and enter the world on Linux

Exit: the complete signed original login UI renders and accepts input; native
transport/authentication, character selection and observed GrimGuzzler world entry
work under unchanged protocol/signature/Warden policy. Settings persist and
resources/workers close cleanly. Follow [issues #93–#100](https://github.com/RovxBot/wowdecomp/issues?q=is%3Aissue+is%3Aopen)
and the [integrated contracts](docs/m3/INTEGRATED_CLIENT.md), not historical
widget-by-widget next-caller ordering.

### M4 — Complete the Linux experience and select modern graphics

Exit: Linux play includes working audio, movies, portable crash diagnostics,
settings/device recovery, repeated startup/world transitions and documented
X11/Wayland/DPI/multi-display coverage. Representative UI/text, terrain,
skinned-model, shader/compression and render-target scenes justify the maintained
Linux/macOS graphics decision. Audit existing Metal implementations and obtain
Apple hardware evidence before committing to a full migration.

### M5 — Implement the maintained renderer and ARM64 core support

Exit: the selected Linux backend renders the baseline scenes/gameplay correctly;
ARM64 core/dependencies select correct architecture paths and a Metal hardware
prototype exists. Preserve legacy shader/material, coordinate/depth and asset
behavior; start with scalar CPU parity and add acceleration only when measured.

### M6 — Deliver a native macOS ARM64 client

Exit: an ARM64 application and every bundled dependency run natively on Apple
silicon with original rendering/gameplay, Retina/input/clipboard/fullscreen,
audio/movies, reconnect and clean lifecycle. Record minimum OS/hardware and
packaging/signing requirements. Early Apple integration may start during M2–M4;
Intel/translated execution does not establish this exit criterion.

### M7 — Backport selected 4.3.4 enhancements

Exit per selected feature: recorded benefit, source/build/address provenance,
dependencies, asset/backend requirements, comparisons and default state. Use
[the ledger](docs/BACKPORTS.md). Independent correctness fixes may land earlier;
visual upgrades remain optional and default off. Sun shafts, liquid/reflection
or batching changes require their own evidence; D3D11 does not provide native
Vulkan/Metal support. M8 does not require every candidate.

### M8 — Stabilize and package the initial release

Exit: reproducible native packages/notices/setup instructions, supported-platform
and driver coverage, and at least a two-hour scenario session per primary native
target covering transitions, combat/effects, Lua/addons, audio/movies, reconnect
and logout. Compare frame times, loading, memory and visuals with the Windows
baseline; document accepted regressions and optional-effect costs. No unresolved
release-blocking corruption, crash or hang remains. Game data and distribution
credentials stay external; signing/notarization is separate release work.

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
