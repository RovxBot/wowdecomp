# 4.3.4 backport investigation ledger

Product compatibility target: 3.3.5a (12340). This ledger records evaluated
candidates, including ones that have not been implemented. Runtime verification
and source inspection are different evidence levels.

## NP-001 — Native filesystem contract references (12340 forks)

- Status: independent native file/path implementation and synthetic fixtures pass
  on Linux x86-64, macOS ARM64 and Windows x64, including Linux sanitizers. M2
  package/async/asset/SDL integration remains outstanding.
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
