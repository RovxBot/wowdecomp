# M2 native host-filesystem work

Started 2026-10-17 after the [reference review](REFERENCE_REVIEW.md). This is the
first M2 work package, not the completed milestone. Host-file compilation and
fixtures passed on Linux x86-64, macOS ARM64 and Windows x64, including Linux
ASan/UBSan. Archives, BlizzardCore's stacked/streaming filesystem,
Services async I/O, asset decoders, nearby model allocations and SDL are not yet
integrated. The full Linux/macOS client gate remains in place.

## Interfaces and ownership

`WowNativeFiles` implements a host-file subset of the existing `Blizzard::File`
and `OsFile` interfaces against the M1 native ABI. The target's explicit source
list contains common file/path code plus one POSIX or Windows comparison provider.
The legacy Windows client retains its existing sources and public declarations.
Do not mix native-runtime and legacy-client objects in one executable.

The native `File.h` exposes only implemented operations. File-stack internals,
archive/streaming operations and their error-stack/category API are excluded.
Failures currently report Win32/Storm-style codes through `SErrGetLastError`,
with native `errno` retained for POSIX diagnostics. This does not yet implement
the legacy BlizzardCore file-stack error categories. No original address or
`VERIFY` claim is attached to these new implementations.

Each stream owns a handle, path and mutex. Close releases them; callers must stop
all users before Close and must not reuse a closed pointer. Reads/writes on a
live stream serialize access. The logical cursor advances only by bytes actually
transferred; positioned transfers do not change it. EOF is a successful zero-byte
read. Invalid sizes/offset extents fail before I/O and clear the output count.
Partial transfers remain visible to the caller. POSIX interrupted transfers retry;
hard failures preserve the logical cursor. Flush and resize use real OS services.

Offsets and sizes are 64-bit; native `OS_FILE_DATA::size` is also 64-bit. These
are host ABI structures, not serialized layouts. POSIX creation time is zero
when unavailable (Linux `stat` has no birth time); ctime is not misreported as
creation. Metadata is refreshed from the handle rather than trusting stale cache.

The raw File open flags retain the existing access/share/disposition mapping.
The Os adapter implements all five creation dispositions explicitly, including
TRUNCATE_EXISTING without creation. Read-only truncation fails before mutation.
POSIX share bits are compatibility hints, not mandatory interprocess share locks;
callers needing those locks need a separate implementation. Unsupported flags
fail before opening a file. Unbuffered/direct I/O, temporary attributes,
attribute-clearing recursive deletion and native download-folder discovery are
explicitly unavailable in this subset.

## Paths and data ownership

Generic host operations accept valid UTF-8 and convert engine separators at the
host boundary. They do not rewrite addon locations or lowercase filenames.
No MPQ-internal name, hash, archive priority or signature behavior changes here.

`Blizzard::File::Native::ConfigurePaths` takes explicit data, settings and cache
roots, resolves existing symlinks, rejects overlapping roots, verifies the data
directory and creates only the writable roots. It does not discover or alter a
game installation. Environment defaults and application-shell wiring come later.
`OpenData` returns a read-only stream. User-path resolution routes settings/cache
separately and rejects absolute names, parent traversal and symlinks escaping the
chosen root. Callers create any desired user subdirectories explicitly.

Data lookup walks components with ASCII case-insensitive comparison and rejects
multiple matches, including an exact spelling with a differently cased sibling.
Non-ASCII characters retain their UTF-8 spelling; Unicode case folding is not
claimed. A case-insensitive host filesystem cannot represent distinct colliding
names; such data needs a separate case-sensitive installation for diagnosis.
Resolution assumes stable directory/symlink topology. It is path routing, not a
security boundary against another process racing renames or symlink changes.

## Validation and remaining M2 work

All compilation runs in [M2 file-service Actions](../../.github/workflows/m2-files.yml).
The guarded [runner](../../Tools/Porting/run_core_checks.py) adds native-files
presets for Linux x86-64, macOS ARM64, Windows x64 comparison and Linux ASan/UBSan.
It audits the generated source graph, runs the M1 checks plus `FileServicesCheck`,
records runner/compiler/source/binary identity and retains detailed CTest logs.
The macOS job requires real ARM64 execution and checks executable architectures.
These are synthetic file-service targets, not native game executables.

Fixtures exercise creation/disposition preservation, invalid flags, EOF/partial
reads, positioned/concurrent access, cursor lifetime, resize and sparse offsets
beyond 4 GiB, no-replace move races, copy/attributes, Unicode names, separate
settings save/reload, case ambiguity, symlink escapes, error isolation and callback
termination. A Linux-only linker interposer injects interrupted, short and hard
pread/pwrite outcomes into the actual production provider. Other platforms label
that injection unavailable. Case-collision and symlink fixture availability is
reported according to the runner/filesystem; absence does not establish coverage.

Final file-service implementation/checkpoint:
`95197b3842132c9547d3c415902ca8d8774cdac3`, in
[PR #5](https://github.com/RovxBot/wowdecomp/pull/5).
[Run 38992957309](https://github.com/RovxBot/wowdecomp/actions/runs/38992957309)
passed the generated-graph audit, all 13 Python source/audit fixtures and all five
CTest checks in each configuration. The
[durable file evidence](file-services.json) retains exact source/run/artifact,
toolchain, runner, executable architecture/hash and fixture-availability records.
Downloaded `NativeCore` and `FileServicesCheck` binaries matched their manifest
hashes and ELF x86-64/PE x64/Mach-O arm64 identities. They were inspected, not
executed locally. The macOS job executed all four test/tools as ARM64 binaries.

| Hosted configuration | Compiler | File fixture and core checks | Coverage limits |
| --- | --- | --- | --- |
| Linux x86-64 | Clang 18.1.3 | 5/5 CTest passed | All file fixture groups ran, including case collisions, symlinks, FIFO rejection and syscall fault injection |
| Linux x86-64 ASan/UBSan | Clang 18.1.3 | 5/5 CTest passed | Same file fixtures; no reported sanitizer failure |
| macOS ARM64 | Apple Clang 15.0.0.15000309 | 5/5 CTest passed on ARM64 | Case-colliding files unavailable on the runner filesystem; syscall injection is Linux-only; symlink/FIFO checks ran |
| Windows x64 native ABI | MSVC 19.44.35229.0 | 5/5 CTest passed | Case-colliding files unavailable; syscall injection/FIFO fixture are POSIX/Linux-specific; symlink checks ran |

The initial Windows fixture at `a6ee451` compiled but timed out in
[run 38992737083](https://github.com/RovxBot/wowdecomp/actions/runs/38992737083).
The final fixture explicitly marks its large Windows file sparse with
[FSCTL_SET_SPARSE](https://learn.microsoft.com/en-us/windows/win32/api/winioctl/ni-winioctl-fsctl_set_sparse)
before exercising the production resize/read/write provider. It passed in
0.10 seconds; the 45-second test timeout was not increased. No sparse-file policy
was added to production file creation.

The unchanged M1 subset also passed all four configurations in
[run 38992957316](https://github.com/RovxBot/wowdecomp/actions/runs/38992957316).
The final-code PR file matrix passed in
[run 38992959489](https://github.com/RovxBot/wowdecomp/actions/runs/38992959489),
and the pinned-15595 source audit passed in
[run 38992959481](https://github.com/RovxBot/wowdecomp/actions/runs/38992959481).
The full Windows `WowClient`/`Wow`/`XvidDecoder` build passed with signature checks
enabled in [run 38992959499](https://github.com/RovxBot/wowdecomp/actions/runs/38992959499).
[The durable Windows record](windows-regression.json) preserves the PR merge
checkout's exact source identity separately from head `95197b3`; all three
downloaded binary hashes and PE x64 identities were verified. This is compilation
regression evidence, not a new Windows runtime session or native-game validation.

Local checks are source-only: Python source/build-graph fixtures, workflow/preset
parsing, link/whitespace checks and the runner's refusal to compile locally.
Locally, 12 Python fixtures passed and the CMake selection fixture was skipped
because CMake is unavailable. In Actions all 13 ran and passed.
No game data is embedded or accessed by these fixtures.

Next work remains package/MPQ and async integration, relocation-safe M2 model
storage, representative unchanged-12340 decoding, then SDL3 shell/input and user
path integration. M0 data/server evidence remains partial. Do not mark M2 complete
from this subset's compilation or fixture results.
