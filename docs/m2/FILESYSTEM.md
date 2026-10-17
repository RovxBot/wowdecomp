# M2 native host-filesystem work

Started 2026-10-17 after the [reference review](REFERENCE_REVIEW.md). This is the
first M2 work package, not the completed milestone. Hosted compilation/fixture
results are pending. Archives, BlizzardCore's stacked/streaming filesystem,
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

Local checks are source-only: Python source/build-graph fixtures, workflow/preset
parsing, link/whitespace checks and the runner's refusal to compile locally.
No game data is embedded or accessed by these fixtures.

Next work remains package/MPQ and async integration, relocation-safe M2 model
storage, representative unchanged-12340 decoding, then SDL3 shell/input and user
path integration. M0 data/server evidence remains partial. Do not mark M2 complete
from this subset's compilation or fixture results.
