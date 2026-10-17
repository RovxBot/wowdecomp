# 4.3.4 backport investigation ledger

Product compatibility target: 3.3.5a (12340). This ledger records evaluated
candidates, including ones that have not been implemented. Runtime verification
and source inspection are different evidence levels.

## NP-001 — Native filesystem contract references (12340 forks)

- Status: independent native file/path implementation and synthetic fixtures pass
  on Linux x86-64, macOS ARM64 and Windows x64, including Linux sanitizers. M2
  package/async/asset/SDL integration followed in PRs #6–#8; see NP-002 and the
  [M2 handoff](m2/HANDOFF.md) for current evidence and acceptance limits.
- References: RosemyneH/wowdecomp `d91c854bc9c6af8d27e6e4093d76985671893bc5`,
  Arlyon/wowdecomp `24778cd77a6a217bbfdffccb88d9556a06b8b21d` (`macos`), both
  3.3.5a/12340 forks. Full source links and findings are in the
  [M2 reference review](m2/REFERENCE_REVIEW.md).
- Inspected contracts: File/Filestack open/read/write/metadata/move operations,
  `OsFile` adapters and Darwin alignment/no-replace filesystem leaves. Their
  POSIX/OS separation informed this design; implementation bodies are newly
  written, not imported, and have no donor binary-address/verification claim.
- Destination: `BlizzardCore/Portable/File*`, `Os/Portable/File.cpp`, explicit
  `WowNativeFiles` target and synthetic `Tools/PortableFiles` fixtures.
- Adaptation: M1 native-runtime ABI, opaque per-stream state, checked 64-bit I/O,
  deterministic case-ambiguity failure and explicit separate host roots. Existing
  Windows implementations remain selected for the full client.
- Assets/defaults: no game assets, network/schema changes, signature-check changes,
  addon redirects, renderer changes or optional effects. Native files are opt-in.
- Validation/limits: [filesystem design and evidence](m2/FILESYSTEM.md). No source
  inspection or native file fixture establishes gameplay or completes M2.

## NP-002 — Native package, worker and SDL3 shell contracts (12340)

- Status: independently implemented native boundaries merged in
  [PR #6](https://github.com/RovxBot/wowdecomp/pull/6),
  [PR #7](https://github.com/RovxBot/wowdecomp/pull/7) and
  [PR #8](https://github.com/RovxBot/wowdecomp/pull/8). Physical input acceptance
  and full game integration remain distinct; see the [handoff](m2/HANDOFF.md).
- References: the same RosemyneH and Arlyon 12340 commits as NP-001, plus this
  checkout's original `Client::OpenArchives`, `PatchFiles`, MPQ/SComp readers,
  signature verification, Services async queue, Os events and Windows input/IME
  callers. The forks informed POSIX/SDL boundaries; no implementation body or
  graphics subsystem was imported from either fork or the 15595 donor.
- Destination: `BlizzardCore/Portable/Package*`, `Assets*`, `Asset*`,
  `Os/Portable/ModelArena*`, `Services/Portable/AsyncFile*`, `ShellSettings*`,
  `Os/Portable/Window*`, `Tools/NativeAssets` and `Tools/NativeShell`.
- Adaptation: explicit little-endian 12340 decoding, original archive precedence,
  original-key RSA and selective MD5 policy, bounded signed-relative model arena,
  retained typed tickets with owner-thread completion, and SDL3 main-thread input.
  The native APIs do not claim legacy object layouts or original binary addresses;
  unchanged MPQ encryption and legacy Windows paths retain their provenance.
  Native `stpl.h` type diagnostics use standard `name()` behind guards; legacy
  `raw_name()` expressions remain intact. Existing Windows cursor fixes remain.
- Dependencies: SDL 3.2.28 at `7f3ae3d57459e59943a4ecfefc8f6277ec6bf540`,
  archive SHA256 `7a8347c770b90b33daac2352858ca03f9c9a2ccc8ce711054870361d1a6b32e5`;
  existing bundled zlib 1.2.2, bzip2 1.0.2, blast 1.1 and LZMA SDK 4.40.
  Sources/notices remain attributed. Native blast calls serialize its static
  first-use tables and always decompress `MPQ_FILE_IMPLODE`, matching the original
  caller even when encoded length is at least output length. Legacy codecs stay
  unchanged. Focused concurrent and encoded-size fixtures cover these fixes.
- Assets/defaults: unchanged external retail 12340 data; no new assets or shaders.
  No optional visual effect is enabled. Native targets are opt-in and keep their
  ABI separate. Required verification remains enabled. Trial/network-streaming
  installs are unavailable; no fake-success replacement is provided.
- Validation: [hosted identities](m2/shell-builds.json), [asset evidence](m2/ASSETS.md)
  and [desktop evidence](m2/shell-runtime.json). Linux release/ASan/UBSan, Windows
  x64 native ABI and actual macOS ARM64 fixtures pass. Linux original-data desktop
  execution is distinct from dummy-driver fixtures, physical input, Gx rendering
  and gameplay; none establishes the later Vulkan/Metal graphics decision.

## BP-001 — Correct CPUID structure initialization

- Status: source-confirmed candidate; not applied or runtime-tested during M0.
- Product revision: `cd29da38b9162419ee0fa29c6b16e07e10f6c4f0`.
- Product function: `System_Hardware::ReadCPUID`, tagged 12340 address `0x0042E120`,
  in [Hardware.cpp](../Engine/Source/BlizzardCore/Source/System/Hardware/Win64/Hardware.cpp).
- Observed defect: `memset(info, sizeof(info), 0)` supplies a zero byte count and
  therefore does not initialize the output structure. `sizeof(info)` is also the
  pointer size, not the structure size. This conclusion follows from the call's
  arguments; it is not a demonstrated gameplay failure.
- Donor: [wowdecomp15595 at `959b0b85c5f55c565328d7c3ca33027a65e41327`](https://github.com/SatyPardus/wowdecomp15595/blob/959b0b85c5f55c565328d7c3ca33027a65e41327/Engine/Source/BlizzardCore/Source/System/Hardware/Win64/Hardware.cpp),
  same function, tagged 15595 addresses `0x007AD1B0` (x86) / `0x1404A2EB0` (x64).
- Relevant donor behavior: `memset(info, 0, sizeof(CPUIDInfo))`. The rest of its CPU
  detection algorithm and dependencies differ and should not be imported wholesale.
- Intended adaptation: an independent initialization fix with a focused check for
  initialized fields/early-return behavior, plus Windows Actions compilation. Keep
  portable/ARM64 CPU detection as a separate M1/M5 task.
- Assets/protocol/defaults: no asset changes, packet changes or feature flag needed.
- Supported backends: CPU-only fix; graphics backend independent.
- Evidence needed before landing: isolate output initialization, check early-return
  behavior without requiring a specific host CPU, preserve/refresh verification
  annotations, run the Windows baseline build in Actions and record results.
- Disposition: queued for a small implementation change after the M0 inventory.
