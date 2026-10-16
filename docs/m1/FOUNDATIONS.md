# M1 build isolation and portable core

Started 2026-10-16 on `native-port/m1-foundations`, based on M0 handoff
`8846d9b4e9dc6684ca167a45e79f324df059b8b5`. M1 is in progress. M0 remains partial;
its Windows runtime, clean-data and live-server observations are still required.
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

## Hosted validation

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

## Runtime handoff and remaining M1 work

This work package does not satisfy M1's runtime exit criteria. In particular:

1. Port allocation/alignment and error/log handling through existing Os, Storm and
   BlizzardCore boundaries. `SMem` owns global new/delete, rounds allocations and
   uses `_msize` for realloc/zero-fill behavior. Specify overflow, failure and
   ownership semantics before replacing it. Track the existing `SIMD::AllocA16` /
   `FreeA16` pairing separately; it is not changed here.
2. Port time, secure randomness, TLS, threads, atomics and locks with focused
   semantic checks. Storm supports recursive locks, events/semaphores, wait-all /
   wait-any and explicit thread lifetimes; BlizzardCore has separate lock/thread
   interfaces. Test timeout/notification/shutdown behavior before using them in I/O.
3. Complete startup/global-constructor audit. The existing Windows `start()` calls
   `StormRtlInitialize()` before CRT startup; that initializes logging then memory.
   Windows `WinMain` switches main onto an 8 MiB fiber and sets floating-point/CRT
   behavior. Storm logging/lock globals and process-wide allocation must remain
   valid during constructors and teardown. The core test uses ordinary CRT `main`
   with none of those client globals; it is not a replacement client entry.
4. Design nearby allocation before loading M2: `M2Array` keeps signed 32-bit
   relative offsets, and x64 `OsAllocNear` reserves/commits memory within a bounded
   range of its owner. Preserve ownership, range and failure behavior; generic
   malloc is not a substitute. No M2 representation or allocator changes here.
5. Continue calling-convention, SEH, intrinsics, LP64/LLP64 and shared-header audit.
   No global structure packing or Windows-on-POSIX compatibility macros are added.

The queued CPUID fix remains a separate candidate in [the backport ledger](../BACKPORTS.md).
M1 completes only after core services and a minimal native client lifecycle are
implemented and checked, with a passing Windows regression build.
