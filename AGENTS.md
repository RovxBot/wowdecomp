# Project instructions for coding agents

## Mission and reading order

Build a native 3.3.5a (12340) client for Linux x86-64 and macOS ARM64, retaining
Windows x64 as the regression reference. Use 4.3.4 (15595) as a reference and source
of selected improvements. Read [PORTING_PLAN.md](PORTING_PLAN.md) and the relevant
module/build files before changing implementation. [README.md](README.md) describes
the Windows client and hosted native targets; the full native game is still gated.

Follow the user's current scope and instructions. The plan guides implementation;
it does not turn a documentation request into authorization to start the whole port.

## GitHub issues and documentation

Use the [repository issue tracker](https://github.com/RovxBot/wowdecomp/issues)
for the current work queue, open bugs/blockers, acceptance criteria, progress and
validation evidence. This workflow supersedes the practice of appending task
handoffs and per-PR execution histories to this file or large Markdown reports.
Existing validation manifests and pinned history remain evidence and contract
references; their old next-step lists are not the current task queue. Issues do
not override the user's scope or standing compatibility, build and data-safety rules.

### Read issues before choosing or continuing work

- Before starting or resuming implementation, search relevant open issues and read
  their bodies, latest comments, dependencies and linked PRs/evidence. Use explicit
  repository-scoped issue searches, excluding PRs when finding the issue queue.
  Search relevant closed issues too when investigating a regression, prior decision
  or already-completed prerequisite. Do not rely on issue titles alone.
- Choose the next concrete step from the user's request and the issues' current
  targets, acceptance criteria and blockers, within the plan's integration direction.
  Read the relevant source/build files and durable contracts as well. If issue
  claims conflict, check the underlying commits and execution evidence; a newer
  comment alone does not establish a newer qualified result.
- Recheck the working issue before posting a handoff or completion report so that
  another agent's updates are incorporated. If tracker access is unavailable,
  report the specific access gap, continue independent authorized work and include
  a concise pending issue update in the final handoff. Do not claim it was posted
  or start another Markdown work log as a substitute.

### Keep issues actionable and log evidence there

- Search for an existing issue before creating one. Reuse the relevant issue for
  a cohesive implementation task, or create one if none covers the authorized work.
  Create linked follow-up issues for distinct unresolved bugs, blockers or
  acceptance gaps discovered during that work. Avoid duplicate issues, one issue
  per routine test run and a single unbounded milestone diary.
- Keep the issue body concise: problem/target, scope, acceptance checklist,
  dependencies and current blocker/next step. Add dated comments for meaningful
  findings, decisions and validation, then refresh the current summary. Link
  related issues and PRs in both directions; use the repository's existing labels
  and milestones where applicable.
- Evidence comments must identify the implementation commit and the tested
  checkout/artifact source when different, exact Actions run/job/attempt links and
  artifact identities/hashes where applicable, reproduction commands, relevant
  platform/data/configuration and observed results. Record failures, cancellations,
  skipped checks and remaining gaps as well as passes. Retain earlier failed or
  superseded evidence and explain which later result supersedes it and why.
- Keep hosted compilation, authored fixtures, debugger inspection, strict
  sanitizer execution, original-data rendering, operator input and server/gameplay
  observations distinct. State the integrated executable's actual observed stop
  and next concrete blocker separately from component-fixture success.
- Link detailed logs/captures as retained artifacts or attachments and include
  enough identity/result information in the issue to remain useful after Actions
  artifacts expire. Keep game data, credentials and sensitive session content out
  of issues as well as Git. Link existing machine-readable validation manifests
  instead of duplicating them; add new manifests only for tooling or reproducible
  evidence needs, not as a parallel status log.
- Before ending work, update the issue with the outcome, checks, remaining gaps
  and next action; include its link in the PR and final handoff. Close an issue
  only when its acceptance criteria are evidenced. A merged PR or passing build
  alone does not close runtime/input/gameplay requirements; avoid auto-closing
  references when the PR leaves required validation outstanding.

### Keep repository documentation durable and concise

- Reserve `AGENTS.md` for agent rules, `PORTING_PLAN.md` for strategy/dependencies,
  `README.md` for user-facing status/usage, and module docs for lasting architecture,
  contracts and build/run instructions. Update them when those facts change, with
  short links to the tracking issues for current work and evidence.
- Do not append per-run results, full SHA/workflow inventories, repeated milestone
  narratives or new per-PR handoff sections to Markdown. Keep provenance/notices
  and actual imports in the existing backport ledger; link issue evidence rather
  than copying execution histories into multiple files.
- Preserve existing historical reports and exact identities. When moving active
  work into an issue, link the relevant document/section and summarize only the
  current target, blocker and acceptance criteria. Do not bulk rewrite or delete
  historical evidence as part of an unrelated implementation change.

## Current integration priority — 2026-10-25

Use the [current integration direction](PORTING_PLAN.md#current-integration-direction--2026-10-25)
as the strategic constraint and the issue workflow above to choose the next
concrete task. It supersedes historical next-caller/widget ordering in the handoffs;
those reports retain their original evidence and implementation identities.

- The product uses unchanged clean 3.3.5a/12340 game data with our compiled native
  Linux x86-64 executable or macOS ARM64 application. Reuse the reconstructed
  client, original Lua/XML interfaces and gameplay logic. Keep both reference
  installations read-only, with settings/cache/logs outside them.
- Prioritise integrated original-client startup and the complete original login UI,
  then native transport/authentication, character selection and observed world
  entry. Bring up the original engine/Event/Frame/Glue callers together; implement
  reachable missing widgets/services in coherent batches driven by that path.
  Exhaustive standalone qualification of every widget is not a prerequisite to
  attempting integrated startup.
- Prefer adapting existing engine-facing interfaces and reusing qualified native
  services. Bounded source compatibility helpers are allowed when they preserve
  required widths, semantics and ownership. Do not require a new independent
  service/owner/probe for every original module. Retain established lifetime,
  alignment, callback and shutdown corrections when connecting the client.
- Audit the pinned Apple-port references in the plan for reusable startup,
  platform, network and Metal code before independently implementing equivalents.
  Review dependencies, provenance/notices and target compatibility; record actual
  imports. Metal code requires a separate Linux graphics path. macOS integration
  may begin as dependencies and hardware permit without waiting for all Linux
  widgets or later renderer/release milestones.
- Keep required hosted checks, Windows regression compilation and sanitizers.
  Add focused regression coverage for changed contracts and observed failures;
  batch validation around coherent integration changes. Report the integrated
  executable's observed behaviour and next concrete blocker separately from
  component-fixture success. Existing probes remain useful regression tools.
- All compilation still runs in GitHub Actions. Preserve original archive/interface
  signatures, full ordered GlueXML/post-creation verification and protocol
  behaviour. No fake-success services, unchecked login mode or server/Warden
  policy changes follow from this direction. The user signs in interactively
  when ready; login rendering alone does not establish authentication/gameplay.

## Build, data and server policy

- All client and dependency compilation, including experiments, runs in GitHub
  Actions. Source inspection, documentation checks, source-audit Python tests and
  execution of downloaded, hash-verified binaries may run locally.
- Use the existing [workflows](README.md#getting-builds-and-running-diagnostics)
  and [build scheduling policy](docs/CI_BUILD_POLICY.md). Keep applicable hosted
  checks, full signature-enabled Windows regression compilation and strict
  sanitizers. Reuse unchanged build inputs only with recorded identity proofs;
  never describe an older checkout as a final-head run.
- Both `/mnt/games/ChromieCraft` and `/mnt/games/3.3.5aClean` are read-only.
  The former is modified and has case-colliding MPQs/proxy components; do not
  rename or delete them to manufacture stock data. The separate clean reference
  has a [12340/enUS inventory](docs/m2/clean-data-inventory.json); representative
  authenticated reads do not certify every archive or server compatibility.
  Keep all settings/cache/logs external and game data out of Git and ordinary CI.
- GrimGuzzler is the target server. Do not change cluster/server/Warden policy.
  The [server snapshot](docs/m3/SERVER_IDENTITY.md) distinguishes desired tags,
  running image/binary hashes and the differing embedded revision with a `+`
  suffix. Neither a package tag nor a local checkout proves an exact clean
  compiled tree. Observe realm endpoints and Warden/session behavior directly;
  the user signs in interactively when ready. Keep credentials external.
- Preserve original archive/interface signatures, full ordered GlueXML creation
  and mandatory post-creation digest verification. Do not accept original
  agreements automatically or substitute edited Lua/XML, alternate keys,
  unchecked authentication or fake-success services.

## Contracts to read for the affected path

Read the relevant issue first, then the applicable contracts and module/build
files. Module docs and existing JSON manifests retain exact evidence, limitations
and prior failures; completed component fixtures do not establish integration.

| Area | Durable references |
| --- | --- |
| Core/startup/lifetimes | [Runtime design](docs/m1/RUNTIME_DESIGN.md), [foundations](docs/m1/FOUNDATIONS.md) |
| Files/data/async/window | [Files](docs/m2/FILESYSTEM.md), [assets](docs/m2/ASSETS.md), [async](docs/m2/ASYNC.md), [shell](docs/m2/SHELL.md), [desktop lifetimes](docs/m2/DESKTOP_LIFETIMES.md) |
| Graphics/fonts/Frame | [Graphics contract index](docs/m3/HANDOFF.md), then the affected module document |
| Original Glue/model/FFX | [Integrated client](docs/m3/INTEGRATED_CLIENT.md), [Apple startup review](docs/m3/APPLE_STARTUP_REVIEW.md) |
| Transport | [Packets](docs/m3/PACKETS.md), [server identity](docs/m3/SERVER_IDENTITY.md) |
| Imports/provenance | [Backport ledger](docs/BACKPORTS.md), [native reference review](docs/m2/REFERENCE_REVIEW.md) |

Keep device capabilities truthful: 94 vertex constants and one texture unit are
the qualified integrated baseline. Preserve full-span palette preflights, wide
count arithmetic, signed-relative model allocations, aligned ownership and
shutdown corrections. Do not raise limits or discard required effects to bypass
an observed stop. Preserve owner-thread/current-context, layout mutation,
callback, traversal and shutdown guards; admit only qualified operations.

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
- Keep SDL3 behind the engine-facing interfaces. Review the pinned Metal ports
  and candidate Linux/macOS graphics paths before selecting a maintained backend;
  require representative scene/shader evidence before a full migration.
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

Use checks appropriate to changed behavior. Core/async changes need semantic
tests, rendering needs actual backend/scene evidence, and ARM64 claims need
hardware execution. Documentation-only changes need reference/whitespace checks
and no client rebuild. Use synthetic or redistributable fixtures in ordinary CI.

Record commands, results, exact source/artifact identities and skipped checks in
the tracking issue. Distinguish source inspection, compilation, authored fixtures,
debugger inspection, strict untraced execution, original-data rendering, operator
input and server/gameplay. Report the integrated executable's actual observed
behavior and next concrete blocker separately from component success. Keep all
detectors enabled and retain failed attempts, timeouts and external-library
qualifications; no suppressed result becomes a clean pass.

For visual/gameplay comparisons record client/data build, locale, actual server
identity/settings, OS/architecture, GPU/driver and toolchain as applicable. Update
milestone status only when acceptance is evidenced; builds, login activation and
authored pixels do not establish complete login rendering or gameplay.

Complete authorized reversible work without routine confirmation requests. If an
external prerequisite is missing, continue independent work, log the exact gap
and leave the acceptance requirement open.

Historical summaries remain in [pinned Git history](https://github.com/RovxBot/wowdecomp/blob/6a18bde50e300fc34d7ed983a2ed1dc97ef76d58/AGENTS.md)
and the original evidence manifests. See [migration #92](https://github.com/RovxBot/wowdecomp/issues/92);
do not append new execution histories here.
