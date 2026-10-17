# M1 build isolation and portable core

Started 2026-10-16 on `native-port/m1-foundations`, based on M0 handoff
`8846d9b4e9dc6684ca167a45e79f324df059b8b5`. The initial build-isolation work was merged via [PR #2](https://github.com/RovxBot/wowdecomp/pull/2)
on 2026-10-17 after [M0 PR #1](https://github.com/RovxBot/wowdecomp/pull/1).
M1 runtime foundations were merged in [PR #3](https://github.com/RovxBot/wowdecomp/pull/3)
on 2026-10-17 at `7137b7c` after the final allocation-contract correction and all
hosted checks passed. M1 foundation exit criteria are met by the evidence below.
M0 remains partial: operator
Windows results are recorded, with a pre-existing OpenGL failure and incomplete
clean-data/live-server evidence. See [the M1 runtime session](RUNTIME_SESSION.md).
The [M0 manifest](../m0/windows-baseline.json) remains the regression reference:
run `38845726826`, harness `5fece2ca698c53680174a4b8c20948b5c484ab21`, product source
`cd29da38b9162419ee0fa29c6b16e07e10f6c4f0`.

## First work package

- Separate target OS (`WINDOWS`, `LINUX`, `MACOS`), CPU (`X86`, `X86_64`, `ARM64`)
  and pointer width. Keep `WOWDECOMP_ARCH_64` a pointer-width definition only.
- Replace recursive module globs with a collector that enters common directories
  and explicitly selected platform directories. Both Windows executables use the
  selected Windows entry tree. Existing common code still has Windows dependencies;
  selection alone does not make those modules portable.
- Move global Windows definitions/options onto a Windows client interface and
  configure each compiled target explicitly. Preserve Win32 x87/time32 settings,
  the SoundEngine directory's full Windows headers, both custom entries, resources,
  platform libraries, signature-check defaults and crash-logger defaults.
- Add `WowPortableCore`, an explicit allowlist of unchanged Storm `MD5.cpp` and
  `Sha1.cpp`. These existing routines serve asset verification/authentication code;
  the target does not include the full archive, signature or network subsystems.
  No donor code, new external dependency or game asset is introduced. Bundled source
  identity remains the recorded product revision plus this branch's changes.
- Add Release known-answer checks for both hashes: empty input, `abc`, high-bit
  bytes, unaligned input and streaming across 55/56/63/64/65/127/128/129-byte padding
  boundaries. Python `hashlib` generated the synthetic expected byte vectors.
- Add hosted Linux x86-64/Clang, macOS ARM64/Apple Clang and Windows x64/MSVC core
  jobs. The Mac job checks runner/process architecture and the executable with
  `lipo`; it executes the tests on ARM64. This is core fixture execution only.
- Audit CMake's actual generated source/definition/link graph before building.
  The Windows reference workflow also checks selection of both executable entries
  and resources and rejects opposite-width Windows sources.

The minimum CMake version is now 3.21 for preset schema 3 and the existing modern
target commands. Initial compiler gates are MSVC from VS 2022 and Clang/Apple Clang
14 or newer. These are build gates, not a tested game support matrix. No SDL3/bgfx,
audio or decoder version is newly selected in this work package.

## Initial hosted validation (2026-10-16)

All client/dependency compilation stays in GitHub Actions. The guarded entry points
are [the core check script](../../Tools/Porting/run_core_checks.py) and
[the Windows baseline script](../../Tools/Porting/Build-WindowsBaseline.ps1).
The new [M1 workflow](../../.github/workflows/m1-core.yml) and existing
[Windows workflow](../../.github/workflows/windows-baseline.yml) retain logs,
source/run identity, runner/toolchain metadata and generated build graphs. Core
artifacts contain test evidence, not a playable native client. Artifacts expire
after 14 days; record exact run/commit identities below after validation.

Local source/documentation checks:

- `python3 -m unittest discover -s Tools/Porting -p 'test_*.py' -v`: nine source
  audit tests passed; the CMake source-selection fixture was skipped because local
  CMake is unavailable. The fixture uses `LANGUAGES NONE` and never compiles code.
- `python3 Tools/Porting/run_core_checks.py --preset linux-core`: correctly rejected
  local execution before creating a build directory or invoking CMake.
- `git diff --check`: passed.
- Documentation links, workflow YAML and preset JSON: parsed/checked locally.
- Win32 compilation/layout verification: not run; no new Win32 support claim.
- Game startup, rendering and gameplay: not run on any new target.

The three core configurations passed in [run 38848848241](https://github.com/RovxBot/wowdecomp/actions/runs/38848848241)
at `e24dcebfc6815646e0cd5d42e9ef916c8acbcc0c`. Each ran all ten source-audit /
selection fixtures and the CTest hash check, with no skips or failures. Generated
graphs contain exactly the two allowlisted hash sources and the test executable.
The [durable core manifests](core-checks.json) retain compiler/runner identities,
artifact IDs/digests and test-executable hashes.

| Core target | Observed hosted toolchain | Result |
| --- | --- | --- |
| Linux x86-64 | Ubuntu 24.04, Clang 18.1.3, CMake 3.31.6 | Compiled; hash checks passed; ELF x86-64 |
| macOS ARM64 | macOS 14, Apple Clang 15.0.0.15000309, CMake 4.4.3 | Compiled and executed on ARM64; hash checks passed; Mach-O arm64 only |
| Windows x64 | Windows Server 2022, MSVC 19.44.35229.0, CMake 3.31.6 | Compiled; hash checks passed |

The Windows full-client regression passed in [run 38848848237](https://github.com/RovxBot/wowdecomp/actions/runs/38848848237)
at the same `e24dceb` source commit: both `WowClient` and `Wow`, plus
`XvidDecoder.dll`, built in Release/x64 with signature checks enabled. Downloaded
binaries matched all three manifest hashes. The [durable regression manifest](windows-regression.json)
retains run/artifact identities and hashes without replacing the original M0
baseline. A source comparison of 42 module targets against the M0 source lists
found only 60 omitted Win32 translation units, each wholly guarded by
`WOWDECOMP_ARCH_32`; no common source was lost. The two selected Windows entries
and resources also passed the generated-graph check. Runtime remains unverified.

The source audit against the pinned donor passed at the initial implementation
`2de9341f53268123702469dc2a55fd2d0e66dd07` in
[run 38848659296](https://github.com/RovxBot/wowdecomp/actions/runs/38848659296).
Subsequent source changes only corrected the Windows test/audit harness.

The core workflow runs these guarded commands **on GitHub Actions runners only**:

```bash
python3 Tools/Porting/run_core_checks.py --preset linux-core
python3 Tools/Porting/run_core_checks.py --preset macos-arm64-core
# Windows runner uses python rather than python3:
python Tools/Porting/run_core_checks.py --preset windows-core
```

Each invokes `cmake --preset <preset>`, builds `WowPortableCore` and
`PortableCoreCheck`, then runs CTest in the preset's separate `build/` directory.
Use the workflow dispatch UI for repeats; do not run compilation locally.
The default full-client configuration still rejects Linux/macOS; core presets
explicitly turn off `WOWDECOMP_BUILD_CLIENT` and enable only the audited subset.

## Runtime services and native lifecycle (2026-10-17)

`WowCoreServices` now supplies native implementations through the existing Os,
Storm and BlizzardCore interfaces. [Runtime design](RUNTIME_DESIGN.md) records
allocation ownership/alignment/realloc/failure semantics, log/error lifecycle,
wait-all atomicity, event/semaphore/mutex behavior, thread/TLS shutdown ordering,
calendar epochs, stack/FP startup and explicit unavailable capabilities. It also
records the required M2 nearby-allocation design without changing M2 or OsAllocNear.
The [upstream review](UPSTREAM_REVIEW.md) records the retained mouse and tutorial
fixes. No donor backport or graphics change was included.

The core/service/native targets passed in [run 38989058328](https://github.com/RovxBot/wowdecomp/actions/runs/38989058328)
at `9ee86a565a26dda463ea47eedb99d0fe79dc28ea` on Linux x86-64/Clang, macOS ARM64/Apple
Clang and Windows x64/MSVC, plus Linux ASan/UBSan. Each configuration passed all
ten source-audit/selection fixtures and four CTest checks:

- `native-core-lifecycle`: three initialization/shutdown cycles, an actual 8 MiB
  stack, FP startup, early global allocation/logging and late global handle/log
  teardown. This runs the minimal core executable, not CommonMain or gameplay.
- `portable-core-hashes`: unchanged MD5/SHA-1 known-answer/streaming fixtures.
- `portable-core-services`: allocation/realloc/zeroing/overflow/alignment, recursive
  and abandoned mutexes, manual/auto-reset events, bounded semaphores, wait-any/all,
  deadlines, shared-reader/excluded-writer behavior, atomics, concurrent once/TLS
  publication, TLS destruction before completion, early caller-handle closure,
  serialized/append logging and calendar/monotonic time boundaries.
- `portable-random-fails-closed`: invalid random output fails with a fatal diagnostic
  rather than silently returning or using a weak fallback. Ordinary random checks
  exercise buffer delivery/bounds and repeated OS requests, not statistical quality.

[Durable runtime evidence](runtime-validation.json) retains exact toolchain,
runner, artifact, binary hash, architecture and CTest results. NativeCore binaries
from the downloaded artifacts matched their hashes. The macOS job verifies all
three executables as arm64 and runs them on an ARM64 runner. Linux sanitizer
results apply to core services; they do not validate the full game or all legacy
hash internals. No client/dependency compilation ran locally.

The workflow runs `run_core_checks.py` with `linux-core`, `linux-core-asan`,
`macos-arm64-core` and `windows-core`. It builds `WowPortableCore`, `WowCoreServices`,
`PortableCoreCheck`, `CoreServicesCheck` and `NativeCore`, then executes CTest.
Core artifacts now include `bin/NativeCore` (or `NativeCore.exe`) alongside logs
and manifests. NativeCore runs in a writable directory and emits `native-core.log`;
it needs no game data or server. Compilation remains exclusively in Actions.

The final Windows full-client regression passed in
[run 38989058307](https://github.com/RovxBot/wowdecomp/actions/runs/38989058307) at
`9ee86a565a26dda463ea47eedb99d0fe79dc28ea`. Both client executables and XvidDecoder
built with signature checks enabled; the generated graph rejected neither ABI nor
platform selection. Downloaded binaries matched their hashes and PE x64 machine
identity. [The durable Windows record](runtime-windows-regression.json) preserves
that evidence. The pinned donor/source audit also passed in
[run 38989058333](https://github.com/RovxBot/wowdecomp/actions/runs/38989058333).

M1 is **complete for its portable core/build/minimal-lifecycle exit criteria**.
This does not establish a native playable client or complete the M0 runtime/data /
server sign-off. Win32 compilation/layout checks were not run. The 2026-10-17 operator sessions cover the
older M0/M1 Windows artifacts, with D3D9 reported passing and OpenGL failures
matching across both. They do not cover the later upstream cursor change or this
runtime-services implementation.

The complete native game remains gated. Files/MPQ/async I/O, data inspection and
SDL application/input belong to M2; networking/rendering/audio and gameplay remain
later work. Legacy thread pools, process launching and SEH/crash UI are not native
core exports; their callers need explicit integration as those modules are ported.
CPUID BP-001 remains a separate queued backport. Existing Windows implementations,
verification defaults, protocol/assets and M2 relative addressing are preserved.

## Final allocation-contract validation

Final review corrected recoverable failure in native diagnostic/metadata new:
SMem returns null when recovery is enabled, throwing C++ new overloads raise
std::bad_alloc, and nothrow new returns null. The Windows legacy path retains its
existing behavior. Focused overflow/failure fixtures exercise both Storm and
BlizzardCore diagnostic operators.

The final implementation is `b818951ac21c0cd8a21cb3cdb1032db2dbd03481`. It passed
all four core configurations in [run 38990308909](https://github.com/RovxBot/wowdecomp/actions/runs/38990308909),
the Windows full-client regression in
[run 38990308848](https://github.com/RovxBot/wowdecomp/actions/runs/38990308848),
and pinned donor/source audit in
[run 38990308814](https://github.com/RovxBot/wowdecomp/actions/runs/38990308814).
The PR checks also all passed before merge. The core results are appended to
[runtime-validation.json](runtime-validation.json); the verified Windows binary
hashes/PE identities are in [the final Windows manifest](allocation-windows-regression.json).
Earlier phase evidence remains unchanged and labeled by its original source/run.
