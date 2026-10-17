# M2 native packages and asset inspection

Work in progress after merged [filesystem PR #5](https://github.com/RovxBot/wowdecomp/pull/5).
M2 remains open: native asset compilation, real-data execution, asynchronous I/O,
bounded asset decoding and SDL3 desktop validation are not established by source
inspection. All compilation runs in Actions.

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
evidence, not native decoding or gameplay evidence. The native executable must
repeat those checks. No files in either supplied game installation were changed.
