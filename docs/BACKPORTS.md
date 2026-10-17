# 4.3.4 backport investigation ledger

Product compatibility target: 3.3.5a (12340). This ledger records evaluated
candidates, including ones that have not been implemented. Runtime verification
and source inspection are different evidence levels.

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
