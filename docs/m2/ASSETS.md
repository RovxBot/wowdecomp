# M2 native packages and asset inspection

Implemented in [asset PR #6](https://github.com/RovxBot/wowdecomp/pull/6), after merged
[filesystem PR #5](https://github.com/RovxBot/wowdecomp/pull/5). All compilation runs
in Actions. The subsequent [async service](ASYNC.md) and [SDL3 shell](SHELL.md)
are merged; current acceptance and evidence are in the [M2 handoff](HANDOFF.md).

The native package boundary is read-only and separate from the legacy Windows
streaming ABI. It mounts an explicit external Data directory, preserves the
12340 retail patch/locale and expansion-prefix ordering from `Client::OpenArchives`
and `PatchFiles(false)`, and never adds a loose-file override. Trial and network
streaming installs are not yet supported. Backup/base locale MPQs are not mounted
by the original retail archive table and are not mounted here.

MPQ versions 0/1 use explicit little-endian table decoding, 64-bit archive offsets,
checked table/sector extents, original name hashing/encryption, compressed and
uncompressed sectors, single units, deletion markers, and original Adler checks.
The original bundled zlib 1.2.2, bzip2 1.0.2, blast and LZMA SDK 4.40 sources are
selected explicitly. Native wrappers bound PKWARE/sparse output and support the
same compression methods as BlizzardCore's original SComp reader. A member read
is limited to 256 MiB and may be further limited by its caller; exceeding that
limit is a visible failure. Later MPQ formats/delta patches are rejected.

Ordinary package reads require RSA authentication of `signaturefile` with the
original 2048-bit public key, v2 RELEASE/locale/expansion record parsing, and MD5
verification of registered files. The application factory always uses that key;
there is no skip-signature or alternate-key application option. A distinct
synthetic trust anchor allows redistributable fixture datasets to exercise the
entire policy. Fixtures retain only that public modulus and its precomputed
signature; no private signing key or game assets are included. The native scalar
RSA implementation handles public verification only, preserves Storm's integer
and padding conventions, and rejects noncanonical signatures.

The user supplied a separate clean reference on 2026-10-17. Its executable has
fixed version 3.3.5.12340, and its 18 enUS MPQs total 17,264,775,738 bytes. The
[read-only inventory](clean-data-inventory.json) records complete SHA256 hashes,
header metadata and absence of case-colliding MPQ names. These identify the
supplied dataset; they do not independently certify every archive as stock.
A source-only Python probe authenticated its patch-3 signaturefile and read
representative Map.dbc, BLP, HumanMale.m2 and Azeroth.wdt bytes. This is source-audit
evidence, distinct from the native execution recorded below. No files in either
supplied game installation were changed.

## Validation checkpoint

Implementation `2ae79719930c233cc69b34d68f7177c3430a7072` passed all four asset
configurations in [run 38994825630](https://github.com/RovxBot/wowdecomp/actions/runs/38994825630):
Linux x86-64, Linux ASan/UBSan, macOS ARM64 and Windows x64 native ABI. Each ran
eight CTest checks: core/hash/lifecycle, file services, fixture generation, package
policy and scalar decoders. Native architecture checks run on the ARM64 host.

The downloaded Linux release and ASan/UBSan inspectors matched their hosted binary
hashes. Both executed against the external clean reference, authenticated 28,948
registered file MD5s, and decoded Map.dbc (135 rows), a seven-mip BLP, HumanMale
(5,264 vertices), a two-group WMO and its geometry (601 vertices/408 triangles),
Azeroth's WDT (687 populated tiles), and one ADT (256 cells). Applicable registered
file signatures passed; the loading-bar BLP has no manifest record, matching the
original selective registration policy. [Runtime evidence](native-asset-runtime.json)
retains exact build/source/binary identities and decoded output. This establishes
native representative decoding, not rendered scene or gameplay parity.

The first real-data run caught the original manifest's MD5 encoding: four printed
uint32 words, each converted to little-endian bytes by `MD5::ToArray`. The parser
and independent RSA/MD5 fixture now preserve that convention. No verification was
disabled. The initial local LeakSanitizer attempt was blocked by the tool sandbox's
ptrace environment; the checksum-verified hosted binary then ran outside that
sandbox with leak detection and ASan/UBSan enabled, exit 0 and no stderr. No local
client or dependency compilation occurred.

The final asset implementation `376e7aa68cf2a38d6b1ab369705b1e43c4d3d8a3` also
passed original SKIN decoding (6,356 triangles) and a nine-mip terrain BLP
(256x256), with leak detection/ASan/UBSan and no stderr.
[Final runtime record](native-asset-runtime-final.json) preserves its identity.
All four hosted configurations passed [run 38995126319](https://github.com/RovxBot/wowdecomp/actions/runs/38995126319)
and the PR merge checks passed [run 38995128220](https://github.com/RovxBot/wowdecomp/actions/runs/38995128220).
The full Windows reference passed [PR run 38995128256](https://github.com/RovxBot/wowdecomp/actions/runs/38995128256).
PR #6 is merged; no native full-game claim follows from these checks. Native inspection decodes geometry and
pixels; complete animation/material/world-render integration remains M3 onward.

## Native model storage design (before enabling decoding)

The inspection decoder reserves a single owner-scoped virtual address arena
before creating its model header. Both the native relative-array anchors and all
decoded payloads live inside that reservation. This avoids searching near an
already allocated external anchor: the maximum reservation is 1 GiB, so every
valid in-arena displacement fits signed 32 bits. OS reservation uses unoccupied
address space only (`mmap` without MAP_FIXED, or VirtualAlloc MEM_RESERVE), followed
by page-granular commitment. No arbitrary heap fallback is allowed.

Every suballocation checks alignment, addition overflow and full committed extent.
Every relative-array encoding independently checks anchor ownership, target
ownership, multiplication/extent and signed displacement before writing the
original eight-byte count/int32-offset representation. Reservation ownership is
separate from suballocations and lasts through the model owner's destruction.
This native geometry-inspection owner does not transplant the legacy M2Shared
allocator or claim full animation/material/render integration. Existing Windows
OsAllocNear and M2Data relocation remain unchanged.
