# Project instructions for coding agents

## Mission and reading order

Build a native 3.3.5a (12340) client for Linux x86-64 and macOS ARM64, retaining
Windows x64 as the regression reference. Use 4.3.4 (15595) as a reference and source
of selected improvements. Read [PORTING_PLAN.md](PORTING_PLAN.md) and the relevant
module/build files before changing implementation. [README.md](README.md) describes
the Windows client and hosted native targets; the full native game is still gated.

Follow the user's current scope and instructions. The plan guides implementation;
it does not turn a documentation request into authorization to start the whole port.

## Current handoff and build policy

- All client and dependency compilation must run in GitHub Actions, including
  experimental build checks. Do not build the client locally. Source inspection,
  documentation checks and source-audit Python tests may run locally.
- Use [Windows baseline](.github/workflows/windows-baseline.yml) for the existing
  x64 reference and [M0 source audit](.github/workflows/m0-audit.yml) for comparison
  against the pinned donor. Add Linux/macOS Actions jobs as their targets become
  buildable; do not claim unsupported native builds from the current workflows.
- M0 source/dependency inventory and Windows x64 compilation are verified. Operator
  Windows sessions report D3D9 passes and matching pre-existing OpenGL failures;
  data/server identity and captured evidence remain partial. Read [docs/m0/BASELINE.md](docs/m0/BASELINE.md), the
  [durable build manifest](docs/m0/windows-baseline.json), the
  [runtime session record](docs/m0/RUNTIME_SESSION.md) and
  [backport ledger](docs/BACKPORTS.md) before continuing.
- M0 [PR #1](https://github.com/RovxBot/wowdecomp/pull/1) and initial M1
  [PR #2](https://github.com/RovxBot/wowdecomp/pull/2) were merged into `main` on
  2026-10-17. Preserve the reports' exact historical run/commit identities.
- The M0 product source is `cd29da38b9162419ee0fa29c6b16e07e10f6c4f0`, including the
  upstream tutorial bounds fix after the original planning revision. Preserve it.
- Treat `/mnt/games/ChromieCraft` as read-only. It is a modified 12340/enUS data
  candidate with case-colliding MPQs and proxy/patch components; do not delete or
  rename them to manufacture a stock baseline. Establish clean data separately.
- The user supplied `/mnt/games/3.3.5aClean` as a separate clean reference. Treat it
  as read-only too. Its 12340 executable and 18 enUS MPQs are identified in
  [the clean-data inventory](docs/m2/clean-data-inventory.json). Native Linux
  inspection authenticated the original signaturefile and decoded representative
  original assets with ASan/UBSan and leak detection enabled; see
  [asset evidence](docs/m2/ASSETS.md). This does not verify every archive as stock
  or establish server/gameplay compatibility. Keep settings/cache outside both
  installations and keep game data out of Git and ordinary hosted CI.
- GrimGuzzler is the target server. Local deployment sources identify AzerothCore
  with playerbots and enabled Warden; desired manifests do not verify live image
  identity, realm endpoints or gameplay compatibility. Do not change cluster/server
  policy as part of the client port.
- M1 runtime foundations were merged into main via
  [PR #3](https://github.com/RovxBot/wowdecomp/pull/3), with passing hosted core,
  native lifecycle, sanitizer and Windows client checks. M1 core exit criteria
  are met. Read [docs/m1/FOUNDATIONS.md](docs/m1/FOUNDATIONS.md),
  [runtime design](docs/m1/RUNTIME_DESIGN.md) and the
  [upstream review](docs/m1/UPSTREAM_REVIEW.md) before continuing.
- M0 remains partial until complete data/server evidence and the required Windows
  observations/captures are recorded. M1 core success does not establish gameplay.
- M2 host files [PR #5](https://github.com/RovxBot/wowdecomp/pull/5), authenticated
  archives/asset inspection [PR #6](https://github.com/RovxBot/wowdecomp/pull/6)
  and native async files [PR #7](https://github.com/RovxBot/wowdecomp/pull/7) are
  merged. Read the [filesystem work package](docs/m2/FILESYSTEM.md),
  [asset contracts/evidence](docs/m2/ASSETS.md), [async contracts](docs/m2/ASYNC.md)
  and [native-reference review](docs/m2/REFERENCE_REVIEW.md) before continuing.
  SDL3 shell [PR #8](https://github.com/RovxBot/wowdecomp/pull/8) is also merged
  (implementation `2154bed3917ab1322545a365a58c5d2e708906b3`, merge
  `6eecb78e99c889e6f97e55ce9fe5d5855d95ab89`). Read the current
  [M2 handoff](docs/m2/HANDOFF.md), [shell contracts](docs/m2/SHELL.md),
  [hosted identities](docs/m2/shell-builds.json) and
  [desktop runtime evidence](docs/m2/shell-runtime.json). Original-data Linux
  startup/settings/shutdown are observed on X11 and Wayland. The operator confirmed
  the requested physical-input checklist passed on 2026-10-18; the saved final
  Wayland report corroborates real mouse/clipboard activity and clean shutdown.
  See [operator acceptance](docs/m2/operator-acceptance.json). IME language and
  additional display/platform coverage were not identified. M2 exit criteria are
  met for the observed Linux X11 asset/shell baseline as of 2026-10-18. Lifecycle
  [PR #11](https://github.com/RovxBot/wowdecomp/pull/11) selected implementation
  `50a7d91a1794529da6bc094fcf58dc5435392033`, merged as
  `df0f2d0f1f7abf86afd125f9d4d42d37ce542625`. Read the
  [closeout evidence](docs/m2/closeout.json) and
  [desktop lifetime design](docs/m2/DESKTOP_LIFETIMES.md). Exact-source X11 release,
  ASan/UBSan/leak checks, settings relaunch and repeated SDL teardown passed.
  Wayland's remaining Fontconfig report reproduces without engine code or a
  renderer; retain it as a system-library qualification, not a clean sanitizer
  pass. Preserve
  the distinction between operator observations, telemetry and injected fixtures.
  Preserve the distinction between scalar inspection geometry and full
  animation/material/world rendering.
- M3 context [PR #13](https://github.com/RovxBot/wowdecomp/pull/13) selected head
  `2afd390c8fb9f1d7b478c915b5b9d736bcc234d9`, merged as
  `d1dd70aedeae7f09468692503590c4fdbc9423ff`. Read the
  [M3 handoff](docs/m3/HANDOFF.md) and [context evidence](docs/m3/context-validation.json).
  All eight PR workflows passed, including full Windows x64 compilation; both
  Linux GL configurations passed 14 CTest checks. Exact-head release and three
  ASan/UBSan/leak-enabled context/asset cycles passed on AMD RX 7900 XTX,
  SDL X11 on Xwayland, with original 12340/enUS signature authentication and
  loading-bar texture display. The new Gx host context is not yet wired into the
  legacy device/native ABI adaptation. Original login UI, portable networking,
  authentication, character selection and observed GrimGuzzler world entry remain
  open; M3 is not complete. On 2026-10-18 the operator confirmed resize,
  minimize/restore, focus, F11 and close passed on the matching synthetic probe,
  without blank graphics or crashes. Boxes scaled with the window. Keep those
  operator observations separate from automatic GPU telemetry and game rendering.
- M3 original shader service [PR #17](https://github.com/RovxBot/wowdecomp/pull/17)
  selected head `543243c676f16b1a2c32af38d8b0910c73eaadbd`, merged as
  `7c5f5ec4feb59760a25aec07d30dbaa433826da5`. Read the
  [shader contracts](docs/m3/SHADERS.md) and
  [durable validation](docs/m3/shader-validation.json). All eight PR workflows
  passed, including full Windows x64 compilation; both Linux GL configurations
  passed 15 CTest checks. Exact-source release and three ASan/UBSan/leak-enabled
  cycles executed original `0x10003` UI shaders on RX 7900 XTX / Xwayland, checked
  five FBO pixels and displayed the original loading-bar texture, with empty
  stderr and zero workers. Both UI shader members lack registered MD5 entries;
  original-key signaturefile authentication and selective MD5 policy remain
  enabled. This native resource does not expose legacy `CGxShader` objects.
  Keep its original 94-register range and unsupported dialects explicit.
  Preserve its main-thread/current-context ownership and distinguish uploaded
  stereo permutations from rendered stereo, original shaders from original
  fonts/GlueXML, and rendered pixels from gameplay. Full native device/targets,
  login UI/network/world remain open; M3 is not complete. On 2026-10-18 the
  operator confirmed resize, minimize/restore, focus changes, F11 and window close
  all passed on this exact original-data shader probe, with no crash or corruption.
  The matching release report corroborates exit 0, empty stderr and zero workers;
  it has no per-event telemetry or new capture. Keep this operator acceptance
  separate from automatic GPU/sanitizer checks and historical context acceptance.
- Native packages support read-only retail MPQ v0/v1 data. Keep original-key RSA
  authentication and registered file MD5s enabled; the signaturefile prints four
  uint32 MD5 words in the original little-endian conversion convention. The
  synthetic fixture key must never replace the application factory's 12340 key.
- Native file requests retain ownership through exactly one owner-thread callback.
  Cancellation of a current read is cooperative. Do not destroy a queue from its
  callback or another thread, free its buffers early, or substitute its native
  ticket API for legacy CAsyncObject without adapting that caller's contract.
- SDL3 3.2.28 is pinned in the native window target. All SDL operations and event
  delivery belong to the main thread; asset reads belong to the retained worker.
  Native IME events own composition/result snapshots. Keep logical window and
  pixel sizes distinct and release held input/capture on focus loss. The shell
  uses SDL software rendering with GLES2/Metal framebuffer presentation where
  required; this is not the M3/M4 Gx or bgfx prototype. Optional vendor SDKs stay
  excluded and visibly unavailable. Native settings remain separate from WTF.
- Preserve upstream `4119c4b` / `5b41f72` cursor anchoring in both Windows Input.cpp
  files, in addition to the original tutorial bounds fix. The recorded M0/M1
  Windows gameplay sessions predate that input change. Linux SDL acceptance does
  not runtime-validate those Windows implementations.
- The 50 reviewed 12340 upstream commits through
  `b1c71b8bd108823bca952f16c6f83a7ad33b9648` were integrated by
  [PR #15](https://github.com/RovxBot/wowdecomp/pull/15), candidate head
  `aac28edaf5463c15b9612ced9168fa67cbbe1607`, merge
  `d1da8da29f575f4c57434cacab96664b82cef796`. Read the
  [upstream review](docs/upstream/2026-10-18.md) and
  [commit/build record](docs/upstream/2026-10-18.json). All eight workflows passed;
  Windows compilation used separate PR merge checkout
  `88dc92441c2ceac5957888371cd06dee4cf1c275`. Keep `56888bd`'s final attack-power
  rounding helper and `8bf90bc`'s archive behavior correction. Historical 0.5.3
  names without a current mapping stay comments. Original PE/static source
  findings are not new Windows gameplay or native login/world observations.

## Compatibility requirements

- Preserve 12340 authentication, protocol, packet layouts, object fields, gameplay,
  Lua APIs, addon loading and original asset compatibility by default.
- Keep unchanged 3.3.5a data sufficient for the baseline client. Track any optional
  enhancement assets separately and document their supported formats.
- Keep existing file/signature verification defaults. Portability work is not a
  reason to disable checks, suppress server requirements or hide failures.
- Preserve working Windows x64 behavior while adding native targets. Retain Win32
  comparison/layout support where practical; label unavailable checks explicitly.
- Target actual ARM64 execution on Apple silicon. An Intel macOS build or translated
  executable does not establish native ARM64 support.
- Introduce visual upgrades separately from the portability baseline. Optional
  effects start disabled and must leave original behavior intact when disabled.

## How to use the two repositories

The product baseline is this checkout. The initial donor is
[`SatyPardus/wowdecomp15595` at `959b0b85c5f55c565328d7c3ca33027a65e41327`](https://github.com/SatyPardus/wowdecomp15595/tree/959b0b85c5f55c565328d7c3ca33027a65e41327).
The audited local baseline and source findings are in the plan. Keep reference
checkouts outside the product tree; reference commits explicitly.

- Port individual fixes or cohesive features. Do not merge entire donor subsystems
  merely because the source is newer or has higher address-tag coverage.
- Before importing code, identify callers, data layouts, shader/material contracts,
  dependencies and the exact benefit for the 3.3.5a client.
- Record donor repository/commit, original build/address, paths/functions,
  adaptation, asset needs, feature defaults and validation in a backport ledger
  at [docs/BACKPORTS.md](docs/BACKPORTS.md). Preserve upstream attribution.
- Preserve `OFFSET`, `STATIC` and `VERIFY` provenance accurately. Do not present a
  donor address as a 12340 address or carry a verification claim onto changed code
  without revalidation. Annotate substantive adaptations with their source build.
- Remember that shader formats differ: local 3.3.5a uses version `0x10003`, while
  the inspected donor uses `0x10004`. Handle conversion explicitly.
- The donor is also Windows-only. Its D3D11 implementation and WGL OpenGL path are
  references, not completed native Linux/macOS backends.
- Keep Cataclysm JAM networking, gameplay schemas and object-field changes outside
  the 12340 baseline unless the user explicitly expands the compatibility scope.

## Implementation rules

- Work through the plan's dependencies and keep changes small enough to review.
  Keep platform refactors, bug fixes, donor imports and optional effects distinct.
- Prefer the existing `Os`, Storm, BlizzardCore and Gx boundaries. Audit all of
  them: porting only `Engine/Source/Os` will leave Windows dependencies elsewhere.
- Use explicit platform source selection and target-scoped CMake definitions and
  libraries. Do not define `WIN32` on other operating systems to force compilation.
- Separate OS, CPU and pointer width. `WOWDECOMP_ARCH_64` must not imply Windows,
  x86-64, SSE or the Windows ABI on ARM64/macOS/Linux targets.
- Keep Windows headers and handles in platform implementations where possible;
  expose opaque state or portable interfaces to common engine code.
- Use fixed-width types and explicit serialization for disk/network formats. Audit
  `long`, `wchar_t`, alignment, packing, varargs and member-pointer assumptions.
  Do not globally force native runtime structures to the original 32-bit layout.
- Preserve the M2 signed 32-bit relative-offset constraint. `M2Shared` uses
  `OsAllocNear` on x64; replacing it with arbitrary allocations can invalidate model
  data. Record the addressing/ownership design before changing that path.
  The native inspection model uses a separate bounded owner arena containing both
  relative-array anchors and payloads; it does not replace legacy M2Shared.
- Preserve startup dependencies, allocator/log availability during static
  initialization, floating-point behavior and adequate stack capacity. Inspect
  the custom Windows entry before introducing a native entry point.
- Preserve synchronization semantics, callback ordering/thread affinity, partial
  I/O, cancellation, timeouts and shutdown lifetime guarantees. Avoid replacing
  SEH or Windows events with superficially similar behavior without checking callers.
- Distinguish MPQ-internal paths from filesystem paths. Preserve patch/locale
  precedence and account for case sensitivity and writable settings/cache paths.
- Start ARM64 CPU work with correct scalar implementations. Add acceleration based
  on profiling, with scalar parity checks and explicit architecture guards.
- Treat SDL3 and a Gx/bgfx adapter as the plan's defaults to validate. Run the
  representative graphics/shader prototype before committing to a full backend
  migration; document a failed assumption and select an alternative with evidence.
- Keep audio mixing/spatial behavior behind `SE2`. A working device API alone is
  not a complete replacement for FMOD. Native targets cannot link Windows `.lib`
  or depend on Windows DLL loading.
- Keep third-party changes narrow, preserve notices, pin versions and document
  dependency/platform limitations. Add no game data, credentials or build outputs
  to source control.
- Do not make optional peripheral integrations mandatory for native startup. Label
  unavailable capabilities; do not substitute silent success stubs for required
  gameplay, graphics, audio or network behavior.

## Validation and reporting

Use checks appropriate to the behavior changed. Core portability and asynchronous
code need focused semantic tests; rendering needs scene comparisons on real
backends; ARM64 claims need hardware execution. Small documentation changes need
reference/whitespace checks and no client rebuild.

The current Windows baseline commands are executed by the Actions build script,
on its Windows runner only:

```powershell
cmake -S . -B build/x64 -G "Visual Studio 17 2022" -A x64
cmake --build build/x64 --config Release --target WowClient Wow --parallel
```

The full-client CMake configuration still rejects Linux and macOS. The M1 core
presets explicitly disable the client and build only allowlisted hash/core-service
sources and asset-free native lifecycle checks. Use [M1 core Actions](.github/workflows/m1-core.yml) for that
subset; its success establishes a minimal core lifecycle, not full client startup
or gameplay. Do not
invent presets/tests or claim full native support from core-only compilation. Add
native client build/run instructions when the corresponding Actions targets work.

M2 file, asset and async presets/workflows are separate allowlisted native ABI
targets, with synthetic semantic checks on Linux x86-64, macOS ARM64 and Windows
x64 plus Linux sanitizers. [M2 asset Actions](.github/workflows/m2-assets.yml)
produces `NativeAssetCheck`; execute downloaded, hash-verified binaries against
external data with [the runtime wrapper](Tools/Porting/run_asset_inspection.py).
This local execution is permitted; local compilation remains prohibited.
[M2 shell Actions](.github/workflows/m2-shell.yml) builds `NativeShell` with
`linux-window`, `linux-window-asan`, `macos-arm64-window` and `windows-window`.
Use [the shell runtime wrapper](Tools/Porting/run_native_shell.py) for hash-verified
execution with external data and writable user roots; see SHELL.md for commands.
An explicit Linux video-driver request also sets matching child EGL platform;
timeouts are recorded failures, not physical-input or clean-shutdown evidence.
Dummy-driver fixtures do not establish desktop input/IME, usable windows or Gx
rendering. Actual ARM64 fixtures do not establish Cocoa desktop/gameplay support.
Physical input acceptance and the selected X11 M2 baseline are complete. Report
unavailable hardware/session coverage explicitly. The SDL-only
desktop control must stay free of engine/data dependencies. Linux now links its
required display libraries with normal ELF process lifetime ownership: both the
SDL-only control and original-data shell passed X11 ASan/UBSan/leak checks. Keep
`linux-window-dynamic-asan` as a diagnostic comparison. The remaining Wayland
Fontconfig report reproduces without engine code and is retained, not suppressed;
do not claim a clean Wayland LeakSanitizer run.

For implementation changes, record the commands and checks run, their results and
any skipped platform checks. Update milestone status only with evidence. Distinguish
source inspection, compilation, fixture tests, rendered output and observed
gameplay; preserve honest gaps when hardware, data or a server is unavailable.

Use synthetic or small redistributable fixtures in ordinary CI. Keep full game data,
credentials and environment-specific paths external. Record the tested client/data
build, locale, server revision/settings, platform, architecture and GPU/toolchain
for gameplay and visual comparisons. The limited current x64 Warden handler means
server compatibility must be observed, not assumed.

Complete authorized, reversible work without routine confirmation requests. When an
external prerequisite is missing, continue independent work and state the specific
validation gap. Do not mark a milestone complete because its code compiles or its
remaining checks need another machine.
