# M0 baseline and dependency audit

Audited on 2026-10-16. Source/dependency investigation is complete for the initial
porting inventory. M0 runtime sign-off remains open. No client was built locally,
no game data was changed, and no Kubernetes resource or server setting was changed.

## Revisions and reproducibility

| Item | Revision/evidence |
| --- | --- |
| Initial planning baseline | `3adfc71e9f5b123bfc22d7f7d5738e1782de7f55` |
| Product source audited for M0 | `cd29da38b9162419ee0fa29c6b16e07e10f6c4f0` |
| Change since planning | Existing upstream tutorial bounds fix: the reverse search starts at index 59 instead of 60 in `CGTutorial::GetPrevCompletedTutorial`; no portability changes |
| 4.3.4 donor | `959b0b85c5f55c565328d7c3ca33027a65e41327`, branch `w15595` |
| Donor acquisition | Separate shallow reference checkout verified at the pinned commit; local scratch path `/tmp/wowdecomp-m0-15595` is disposable |
| Existing product implementation changes during audit | None; tracked source diff was empty before adding audit/build tooling |
| Build policy | All client/dependency compilation happens in GitHub Actions, as requested by the user |

The repeatable [source audit](../../Tools/Porting/audit_sources.py) examines tracked
files, including tracked binaries that ordinary ignore-aware searches omit. It
classifies same-path source files after removing comments and tokenizing. This
discounts address/verification annotations and whitespace, but is a heuristic, not
a C++ parser or proof of equivalent behavior. Renames appear as additions/removals.

```bash
python3 -m unittest discover -s Tools/Porting -p 'test_*.py' -v
python3 Tools/Porting/audit_sources.py \
  --reference /tmp/wowdecomp-m0-15595 \
  --output /tmp/wowdecomp-m0-source-audit.json
```

Both commands ran successfully; the five classifier tests passed. These are source
inspection tools, not client builds. The [M0 source audit workflow](../../.github/workflows/m0-audit.yml)
repeats the comparison against a pinned, separate donor checkout and uploads JSON.
The scratch checkout/report can be regenerated; neither is a required workstation
installation or committed game asset.

## Build and execution environment

| Check | Observed result |
| --- | --- |
| Local host | Arch Linux x86-64, kernel `6.18.57-1-lts` |
| Available compilers | Clang `23.1.1`, GCC `16.2.1`; not used to build the client |
| Local CMake/Ninja/NASM | Not on PATH; no Python CMake/Ninja package found |
| Windows toolchain | `clang-cl` exists, but no Windows SDK/MSVC build environment was established; not a Windows baseline runner |
| Wine | `11.19` installed; not used and not accepted as native Windows baseline evidence |
| Display/GPU probe | PCI lists AMD Navi 31 and Granite Ridge adapters, but this sandbox cannot open display `:0`; Vulkan enumeration reports `ERROR_INITIALIZATION_FAILED` |
| Windows baseline build | Passed: [GitHub Actions run 38845726826](https://github.com/RovxBot/wowdecomp/actions/runs/38845726826), harness commit `5fece2ca698c53680174a4b8c20948b5c484ab21`; unchanged `cd29da3` game source |
| Hosted toolchain | Windows Server 2022 image `20261012.333.1`, CMake `3.31.6`, MSVC `19.44.35229.0`, Windows SDK `10.0.26100.0` |
| Windows gameplay/OpenGL session | Not executed; no Windows interactive session or clean data staging established |
| Linux/macOS client build | Not attempted; existing CMake explicitly rejects those platforms |

The [Windows workflow](../../.github/workflows/windows-baseline.yml) builds both
`WowClient` and `Wow` in Release/x64 on `windows-2022`, with signature checks enabled
and crash logging enabled. It uploads configure/build logs, a manifest with source,
runner/toolchain and dependency/binary hashes, and `WowClient64.exe`, `Wow64.exe`,
`XvidDecoder.dll`. It does not launch the client or upload game data. The
[build script](../../Tools/Porting/Build-WindowsBaseline.ps1) rejects local execution.
Build artifacts expire after 14 days; preserve the manifest and run/commit identity
when recording later runtime comparisons, and rebuild through Actions if needed.

The downloaded artifact ZIP matched GitHub's SHA-256 digest
`012c8f5f216cdebc8ef1b391ded3aeaa12bdb135cc674d7d8ffb2184a14666d5`. Its three
binaries matched their manifest hashes and have PE AMD64 machine type `0x8664`.
The [durable evidence record](windows-baseline.json) preserves their sizes/hashes,
imports, dependency fingerprints and run/artifact IDs after artifact expiration.
The build emitted 3,737 compiler-warning occurrences, including repeats across
targets/headers; the full log is retained in the artifact. This is an observed
baseline, not a warning-free or runtime-verified build. Binary import inspection
also confirms a VC++ runtime dependency and the companion `XvidDecoder.dll`.

## Supplied client data: useful candidate, stock baseline not verified

The user supplied `/mnt/games/ChromieCraft`. It was inspected read-only. Only a small
allowlist of configuration fields was read; account values and credentials were
not collected. Archive metadata/header checks do not establish the authenticity of
archive contents.

| Observation | Evidence/meaning |
| --- | --- |
| Executable version | PE fixed-file version `3.3.5.12340`, 32-bit x86, 7,704,216 bytes |
| Executable SHA-256 | `4c459de64d38b5795becc6c01a03ba00d34686598fae707c442446722617868a` |
| Executable name alias | `Wow.exe` is a symlink to the same `wow.exe`; not two different builds |
| Configured locale | `enUS`; disk directory is lowercase `data/enus` |
| Configured display | Windowed, `5120x1440`; capture settings must be fixed and recorded for comparisons |
| Server configuration | Both the selected locale realmlist and client config point at the intended GrimGuzzler endpoint |
| Archives | 100 MPQs, 43,586,244,498 bytes across the installation, excluding NAS `@eaDir` metadata; multiple locale directories |
| Expected base archive names | `common`, `common-2`, `expansion`, `lichking`, `patch`, `patch-2`, `patch-3`, and enUS locale/patch archives exist; sampled headers have MPQ magic |
| Extra modifications | Numerous lettered patch MPQs and `AwesomeWotlk*`, patch-menu components and a `d3d9.dll` proxy are present |
| Case collision | `data/patch-E.mpq` (2,323,851,291 bytes) and `data/patch-e.mpq` (261,871,142 bytes) are distinct files |

Do not remove, rename or patch this installation to establish the baseline. A
Windows staging directory cannot faithfully represent the two case-colliding MPQ
names. Obtain or verify a clean 12340 archive set in separate staging, retain the
default verification checks, and treat the existing modified installation as a
separate compatibility scenario. Copying only stock-looking filenames is not
proof that their contents are stock. Record archive hashes/provenance when the
clean dataset is established; full hashing of the 43.6 GB candidate was not done.

## GrimGuzzler target: desired deployment identified, live state unverified

Read-only inspection of the local `homelab-infra` checkout at
`fc872af160573fdc631a211660e5926a6adab030` found the `apps/wotlk` deployment sources.
This records intended configuration; neither live pods/image digests nor the
database realm configuration were queried.

- Server source repository: `mod-playerbots/azerothcore-wotlk`, configured source
  ref `test-staging`.
- Auth/world image tags in the inspected manifests identify source revision
  `9c7395453b194ef677e6e33fc7d0bf05d2b9f26d` in
  `ghcr.io/rovxbot/azerothcore-wotlk`. A different local core checkout is at
  `d6f96d206b0e835fe692ebf4206e8db8a52d4d38`; do not substitute it for the deployed revision.
- Namespace: `wotlk`; realm ID: `1`; auth TCP port: `3724`; world TCP port: `8085`.
  Actual externally advertised world endpoint still needs runtime verification.
- Desired Warden settings: enabled; 3 memory checks, 1 Lua check, 7 other checks;
  check holdoff 30 seconds; response delay 600 seconds; fail action 0.
- The current client x64 Warden handler only consumes data and registers a handler;
  its initializer returns false. Compatibility is not established by a successful
  login or by the configured server fail action. Observe a session lasting through
  the check/timeout window and record server/client outcomes without changing the
  server's policy for the test.

Server source/configuration is external to this client repository. Future work
should obtain the actual runtime image digest/core revision and realm endpoint in
the session record. Credentials, server configs and Kubernetes secrets are not
copied here.

## Third-party and binary dependency inventory

Versions below are declared by bundled headers/source, not inferred from release
dates. Native compatibility is a source assessment; it has not been compiled here.

| Dependency | Bundled version/form | Build use and native-port action |
| --- | --- | --- |
| zlib + blast | zlib `1.2.2`, C source; blast source in `contrib/blast` | BlizzardCore archive decompression; port existing sources first and verify archive behavior before upgrades |
| bzip2 | `1.0.2`, C source | Archive decompression; no binary-only dependency |
| LZMA | SDK `4.40`, C source | Archive decompression; audit integer/size assumptions |
| FreeType | Header declares `2.0`, no patch macro identified | Fonts via Services/Gxu/Game; sources available; native build/config unverified |
| Expat | `1.95.5`, C source | XML/FrameXML; audit build definitions and character configuration |
| PCRE | `5.0`, C source plus local POSIX wrapper | UI/game regular expressions; preserve existing behavior |
| Lua | `5.1.1`, bundled sources compiled as C++ | Preserve 5.1 API/semantics; platform config exists but build flags/CRT helpers require audit |
| JPEG | IJG `6b`, bundled sources compiled as C++ | Image decoding; platform/config build unverified |
| FMOD Ex | `4.09.07`, common sources plus only `win32` platform implementations | Audio mixing/streaming and `SE2`; port the OS/output layer or replace behind `SE2`; donor also uses this FMOD version |
| FMOD codecs/effects | FLAC directory `1.1.4`, modified Ogg/Vorbis, Freeverb/foreverb, OpenAL/VST/Winamp support headers/sources | Exact Ogg/Vorbis upstream releases not established; inventory is pinned by repository revision; CMake selects codec/effect sources and Windows ASIO/output code |
| Xvid | Header `1.3.7`; tracked prebuilt x64 COFF static archive, no core source build vendored | x64 movie decoder; valid Windows dependency, must be rebuilt/replaced for Linux/ARM64 |
| DivxDecoder | Interface header/exports; external 32-bit DLL not in repository | Only required by Win32; x64 uses Xvid replacement; NASM and generated import library belong to Win32 path |
| NVAPI | SDK macro `61683`, headers and tracked x86/x64 Windows libraries | Windows renderer integration; optional capability for native targets |
| Logitech LCD | Header plus local source stubs returning unavailable/service errors | Optional UI integration; not a requirement for a portable shell |
| SteelSeries mouse | HID implementation source; Windows `hid`/`setupapi` link | Optional device functionality; isolate it from required native input |

The Windows SDK/compiler runtime supplies D3D9, OpenGL32, WinINet, Winsock,
DirectInput, IME, WinMM, ACM, version info and debug helper libraries. The existing
Net target also links `wsock32`. These are platform dependencies, not missing
vendored libraries. No SDL3/bgfx dependency has been added yet.

Tracked binary dependency fingerprints:

| File | Bytes | SHA-256 |
| --- | --- | --- |
| `ThirdParty/xvidcore/lib/x64/libxvidcore.lib` | 3,494,900 | `be95947a0d7b3bb6f362bfd5d361ce220c30489b20bc8604c4e3383b286de368` |
| `ThirdParty/nvapi/amd64/nvapi64.lib` | 999,686 | `901e479e548d41c30688c7481f0c6353c0be7fdee978796d27d87c32e19ce172` |
| `ThirdParty/nvapi/x86/nvapi.lib` | 439,838 | `92056c461921c509eaa135c890a47180e822ba8872cfb507c1d452f813a1782f` |

The Xvid archive contains x86-64 PE/COFF objects and MSVCRT link directives; its
presence was checked with `git ls-files`, file metadata, hashes and `objdump`.
Ordinary `rg --files` may omit these tracked `.lib` files due to ignore patterns.

## Engine dependency map and M1 work order

| Boundary | Audited code | Required behavior and handoff |
| --- | --- | --- |
| Build/source selection | Root/module CMake; both executable targets | Windows macros/libraries and `/ENTRY:start` are global or unconditional in several places; recursive globs include both platform trees, often guarded by pointer width only |
| Startup | `WoW/Source/Win64/WinMain.cpp`, `StormRtl.cpp` | Storm logging/memory initialize before CRT startup; WinMain invokes main through an 8 MiB fiber; audit global constructors before replacing entry/stack strategy |
| Allocation/error/log | `Storm/Source/SMem.cpp`, `Win64/SErr.cpp`, `SLog.cpp`, BlizzardCore memory code | `_msize`, process heaps, SEH and globals remain outside `Os`; allocation ownership and initialization order need explicit handling |
| Synchronization | Storm `SLock`/`SCritSect`/`SThread`, BlizzardCore lock/thread code | Recursive critical sections, manual/auto-reset events, wait-all/wait-any, atomics and worker teardown; a plain mutex substitution is insufficient |
| Time/CPU/random/library | `Os/Win64` and BlizzardCore system code | Monotonic versus wall time, Windows time32 calls, x86 CPUID, secure random APIs and DLL loading need separate platform/CPU implementations |
| Files/archive/async | BlizzardCore `System/File`, `Packages/iounit`, `Mopaq`, `Services/AsyncFile.cpp` | Win32 file handles/unbuffered alignment, archive/locale/patch precedence, shutdown events and callback lifetime; normalize host paths separately from MPQ names |
| Packed model memory | `Model2/M2Data.h`, `M2Shared.cpp`, `Os/Win64/OsMemory.cpp` | `M2Array` uses signed 32-bit relative addressing; x64 allocates nearby buffers using VirtualAlloc/VirtualQuery; replacing near-allocation with arbitrary malloc can break offsets |
| Window/input/events | `Os/Win64/Gui.cpp`, `Input.cpp`, `OsIME.cpp`, Event/Scrn/Frame | Windows message pump, focus, IME, clipboard, relative mouse and DPI; bridge through SDL with correct main-thread rules |
| Graphics/shaders | Gx device/common/platform code, OpenGL/GlExt, ShaderEffect | WGL, legacy ARB shaders, D3D9-centric shared code and shader format `0x10003`; keep matching 12340 assets while bringing up Linux |
| Network | `Os/Win64/OsTcp.cpp`, `OsTcpConn.h`, Net, WowServices/WowSvcs | IOCP, OVERLAPPED, Winsock callbacks, partial I/O and cancellation; retain 12340 authentication and wire layouts |
| CPU acceleration | Model2, CharacterComponent, SPIRIT audio and hardware detection | SSE intrinsics and x86 feature detection need scalar/reference paths before ARM64 acceleration |
| Sound/movies/peripherals | SE2/FMOD, XvidDecoder, SteelSeries and UILCD | Preserve sound/mixing behavior; separate movie/library portability and optional peripheral availability |

Start M1 with conditional CMake/source selection and a small portable core target,
then allocator/logging/time/locks and entry-lifecycle investigation. Do not remove
the Windows guard and attempt to compile every platform implementation at once.
The near-allocation/relative-offset constraint is an explicit dependency for M2's
model asset work.

## Donor comparison beyond address tags

The audit counted 3,044 tracked baseline and 3,607 donor C/C++/header/inl files,
including bundled dependencies. Regex indicators in baseline/donor respectively:
Windows header includes 208/208; selected Windows APIs/types 161/167; selected MSVC
extensions 109/159; selected x86 instructions/intrinsics 19/30. These are file-level
triage counts, include conditionally compiled code and do not enumerate every API.

| Module | Identical | Comment/space only | Changed tokens | Donor only | Baseline only |
| --- | ---: | ---: | ---: | ---: | ---: |
| BlizzardCore | 7 | 8 | 100 | 66 | 23 |
| Gx | 6 | 2 | 40 | 29 | 2 |
| Model2 | 2 | 0 | 15 | 5 | 0 |
| Os | 12 | 9 | 44 | 10 | 2 |
| ShaderEffect | 2 | 1 | 6 | 6 | 0 |
| SoundEngine including FMOD | 242 | 206 | 1 | 8 | 4 |
| Storm | 13 | 7 | 44 | 6 | 0 |
| LiquidSystem | 1 | 0 | 9 | 2 | 3 |
| Net | 1 | 0 | 5 | 1 | 0 |
| Ui | 48 | 8 | 246 | 65 | 4 |
| WorldClient | 0 | 0 | 69 | 38 | 43 |

Interpretation from inspected code:

- The donor adds substantial D3D11 and framebuffer/depth-cache/sun-shaft code, but
  its OpenGL shader path still uses `glProgramStringARB`; it is not a GLSL/Metal
  portability solution. No `.bls`/HLSL/GLSL shader assets were found in its tree.
- The donor's `SE3` replaces the engine-facing `SE2` files and depends on newer
  service/data interfaces. Most common FMOD implementation differences disappear
  when comments/address annotations are removed; it does not contain ALSA/CoreAudio
  platform output implementations in this checkout.
- The small same-path FMOD memory-file change uses `mFileSize` where baseline uses
  `mLength`. Treat this as a schema/caller investigation, not an automatically
  applicable bug fix.
- A concrete hardware initialization defect and the donor's corrected initialization
  were identified. See the [backport ledger](../BACKPORTS.md). No candidate was
  applied during M0.

## Outstanding runtime evidence and handoff

Use the [runtime session record](RUNTIME_SESSION.md) with an artifact from a passing
Actions build. Still required for M0 sign-off:

- A separate verified stock 12340 data set and its archive identity/provenance.
- A native Windows D3D9 session and a separate OpenGL session, with logs, fixed-scene
  captures and timing observations; neither ran in this environment.
- The actual live GrimGuzzler core/image identity, advertised realm endpoint and
  observed world-session/Warden behavior. Record desired-versus-live differences.
- Login, character selection, movement/combat/chat/inventory, zoning, settings,
  movies/sound and clean shutdown results, distinguishing existing failures.

M1 source/build isolation can proceed using this inventory while those observations
remain pending. M0 must stay partial until the runtime record is filled with actual
observations. Do not change verification or server settings to manufacture a pass.
