# Native-port references reviewed before M2

Reviewed on 2026-10-17 against product main
`fb8ac98f2ddd40cb7fadc1f00d80f43c0fbb3916`. This is source review and inspection
of existing external evidence. No reference code has been imported, no client or
dependency has been compiled locally, and no M2 exit criterion is newly verified.

Use **Arlyon's POSIX/SDL boundaries and Darwin file providers as the primary M2
design reference**, with RosemyneH's Linux implementation as a second reference.
Adapt individual pieces to the already-validated M1 runtime. Neither branch is a
safe wholesale merge. Both use SDL2; this review retains the plan's SDL3 default.

## Pinned snapshots and evidence

| Reference | Inspected snapshot | Evidence and limits |
| --- | --- | --- |
| [RosemyneH/wowdecomp, main][rose-tree] | `d91c854bc9c6af8d27e6e4093d76985671893bc5` | Full Linux `WowClient` hosted compilation passed. macOS CI only compiles `Os` and `Gx`; it does not build or execute the game. |
| [arlyon/wowdecomp, macos][arlyon-tree] | `24778cd77a6a217bbfdffccb88d9556a06b8b21d` | Documents a native ARM64 `Wow` build, rendered login screen and user-confirmed login/gameplay through DXVK Native/MoltenVK. These are author/operator reports, not reproduction by this project. |

The separate read-only reference worktrees are disposable:
`/tmp/wowdecomp-m2-review-rosemyneh` and `/tmp/wowdecomp-m2-review-arlyon`.
For reproduction, fetch the named repository outside the product tree, check out
the full pinned SHA, and inspect the linked files. Never execute a reference
setup/build script locally as part of this review.

RosemyneH's exact-commit Actions evidence was checked through GitHub's API:

- [Linux run 38985039986][rose-linux-run], job `117254419612`: configure, full
  `WowClient` build and artifact upload succeeded on Ubuntu 24.04. Its “Smoke”
  step runs `file` and `ldd`, not the executable. The workflow uses GCC and system
  SDL2/X11/curl/ALSA/Vulkan/Xvid packages; it does not validate our Clang target,
  input, archive decoding, gameplay or Wayland behavior.
- [macOS run 38985039808][rose-mac-run], job `117254418800`: `Os`/`Gx` static
  libraries built on `macos-14` with `CMAKE_OSX_ARCHITECTURES=arm64`. This is partial
  compile evidence. The [README][rose-readme] explicitly identifies full-client
  macOS gaps.
- [Windows run 38985039952][rose-windows-run]: both x64 and ARM64 build jobs
  succeeded. The ARM64 job is allowed to fail by workflow policy, although it
  passed in this run. These builds do not replace this project's Windows baseline.
- The workflow configure step leaves signature checks at their default enabled
  setting. The README's Linux example explicitly switches them off; that example
  must not become our baseline command.

Arlyon's [macOS bring-up report][arlyon-bringup] and [PR #6][arlyon-pr] describe
Apple M1 Pro/macOS 15.5/Apple Clang 17 execution with 12340 assets. They distinguish
the full client from a standalone D3D9 draw/readback/Present smoke, and report
15 platform checks and two POSIX checks. The latter test time, TLS, recursive
locking, library loading and cloud noise, not MPQ/filesystem/async semantics.
GitHub returned no Actions runs on `macos` and zero check runs for the pinned tip
when queried. The [normal-quit test][arlyon-quit] is useful, but PR #6 reports its
two-renderer execution on the follow-up integration branch. Do not attach that
execution claim to the pinned `macos` tip.

The macOS graphics chain here is **native ARM64 D3D9 calls → DXVK Native → Vulkan
→ MoltenVK → Metal**, not Wine or an Intel executable. Direct Vulkan work is in
the separate `vulkan-macos` follow-up, outside this review's source scope. The
reported gameplay does not establish parity, our data provenance, GrimGuzzler
compatibility or the required Warden observations.

## Pieces worth adapting

| Area | Reference paths/functions | Proposed disposition |
| --- | --- | --- |
| Filesystem boundary | Arlyon `System/File/Darwin/{FilestackIO,FilestackPath}.cpp`; RosemyneH `System/File/Linux/Filestack.cpp` | Adapt the separation of common File/Filestack operations from OS leaves. Darwin code retries interrupted reads, reports actual byte counts, uses `F_NOCACHE` and implements no-replace moves with `renameatx_np(..., RENAME_EXCL)`. Linux needs its own qualified leaves. Keep our target-scoped CMake and M1 core ABI. |
| Archive I/O | Arlyon `Packages/iounit/{IOPlatform.h,Darwin/IOPlatform.cpp,Native/Unbuffered.cpp}` and `Storm/Source/Posix/IOFileUnit.cpp` | Useful explicit alignment, positioned-read, ownership and unsupported-operation contracts. Audit callers and failure cleanup before copying; native `IOFileUnit::Create`, for example, opens a stream before checking whether its container allocation failed. |
| MPQ table expansion | Arlyon `System_SFile::ARCHIVEREC::InitializeTables` in [SFileArchives.cpp][arlyon-mpq] | Investigate early: snapshots each packed 16-byte block-table record with `memcpy` before expanding it in place, avoiding typed aliasing between source words and wider destination fields. The local and RosemyneH versions retain the original typed-read loop. Establish an optimized-build fixture before claiming a demonstrated local failure. |
| SDL event/input boundary | Arlyon [Os/SDL][arlyon-sdl]; RosemyneH `Os/Linux/{Input,Gui,OsClipboard}.cpp` | Adapt to SDL3 behind existing engine APIs. Arlyon separates physical keys from committed Unicode text, drains long text commits without overflowing the legacy queue, releases SDL-owned event text, clears key/button state on focus loss and dispatches window callbacks through the event pump. RosemyneH has additional drawable-size/relative-motion scaling and window teardown work worth comparing. |
| Nearby model memory | RosemyneH [Linux OsMemory.cpp][rose-near] and `Os/Darwin/OsMemory.cpp`; Arlyon [Mac OsMemory.cpp][arlyon-near] | Linux uses `/proc/self/maps` plus `MAP_FIXED_NOREPLACE`; Darwin uses Mach region queries/reservations. Use these as OS API references only. Retain our owner-scoped arena and checked-displacement design described below. |
| Shutdown regression | Arlyon POSIX logger/thread lifetime fixes and [Tools/MacSmoke][arlyon-quit] | Carry the regression scenario forward: close the real SDL window and require normal process exit through late static destructors. Keep the M1 process-lifetime admission/registry design and explicit worker joins. A core-only lifecycle cannot replace this integration check. |

Paths beginning `System/` or `Packages/` in the table are relative to
`Engine/Source/BlizzardCore/Source/`. These are **reference-only candidates** for
12340, with no asset changes or optional effects proposed. Any imported code
needs an entry in [BACKPORTS.md](../BACKPORTS.md) recording the pinned fork,
function/address provenance, adaptation and actual validation. Existing `VERIFY`
comments are not verification of an adapted native body.

## Findings that prevent direct reuse

1. **File reads need semantic fixtures before adoption.** RosemyneH's
   [BlizzardCore ReadNative][rose-readnative] rounds `head + count + 0xFFF` in
   `uint32_t`, so large requests can wrap. Its File/Filestack reads and writes do
   not retry `EINTR`. Its [Storm unbuffered read][rose-storm-read] replaces the
   actual read count with `min(size, chunk)`, allowing a short successful read to
   copy/report bytes that were not read. That last pattern also exists in our
   legacy Windows implementation; it is not evidence of a new fork regression.
   Preserve the distinction between APIs that allow partial success and APIs that
   require a complete request, and isolate any Windows bug fix from portability.

2. **Path normalization does not solve case collisions or writable roots.**
   RosemyneH's [OpenNative][rose-path] redirects missing `Interface/AddOns` reads
   into `Interface/WasmAddOns` and takes the first case-insensitive directory
   match. That changes addon lookup and can select an ambiguous file. Arlyon's
   Darwin providers translate separators but use ordinary exact-case host lookup.
   Neither inspected bring-up establishes our read-only data root plus separate
   user config/cache layout. Define host-path resolution separately from MPQ
   names, preserve archive/locale ordering, and diagnose ambiguous case matches.
   Do not resolve the existing candidate installation's MPQ collisions by renaming
   or deleting data.

3. **Neither nearby allocator completes our model-memory contract.** The inspected
   allocators map individual payloads around an anchor; they are not owner-scoped
   arenas. Size/page rounding and anchor-range arithmetic need checked overflow;
   mapping proximity alone does not validate every actual `M2Array` displacement.
   Our [M1 design](../m1/RUNTIME_DESIGN.md#m2-nearby-model-allocation-design) still
   applies: retain reservation ownership through model unload, verify the whole
   allocation extent and every signed 32-bit encoding, never overwrite occupied
   mappings, and fail the load when the range cannot be satisfied. Use synthetic
   relocation, boundary, collision, allocation-failure and unload fixtures before
   enabling native M2 model loading.

4. **The selected macOS startup has integration gaps.** Arlyon's
   [WoW/Source/CMakeLists.txt][arlyon-entry-build] selects
   [Mac/main.cpp][arlyon-entry], not `Posix/Main.cpp`. The selected main initializes
   SDL on the process main thread, but does not invoke `NativeStartupEnsureStack`
   or check actual stack capacity as our M1 lifecycle does. It also discards
   `argc`/`argv`, while the selected [SCmdProcessCommandLine][arlyon-command] reads
   `/proc/self/cmdline`; Mac `CommandLineUTF8` separately contains an empty string.
   Preserve real native arguments and the existing startup/FP/stack guarantees
   when integrating SDL. Keep SDL ownership on the macOS main thread.

5. **Provider selection needs an audit beyond Os.** Arlyon's
   [WowServices CMake][arlyon-net-build] retains both `Mac/` and `Posix/` providers
   on Apple. `Mac/WowConnection.cpp` and `Posix/Callbacks.cpp`, for example, both
   define `WowConnection::PeerAddrToStr` and other methods. Static archives can
   hide duplicate providers during linking. Select exactly one implementation
   per interface; a linked executable is insufficient evidence of which one ran.
   This is principally an M3 networking concern, and another reason to extract
   M2 file/SDL work separately.

6. **Do not replace already-tested core services or inherit unrelated changes.**
   RosemyneH's [random provider][rose-random] falls back from `getrandom` failure to
   hashes of clocks/process/system state. Retain our fail-closed CSPRNG. Its WASM
   addon layer, job-pool/performance rewrite, extended view and Vulkan rendering
   changes are outside M2. Arlyon's explicit local-only streaming provider returns
   unavailable for downloads; that is not a replacement for local async archive
   work. Its `x86`/`Win` authentication tags must be assessed separately with
   network/security behavior, not copied as part of filesystem porting.

Both SDL adapters require SDL2-to-SDL3 API/ownership review and hardware checks for
IME, non-Latin input, focus, relative mouse, DPI and clipboard. RosemyneH's legacy
IME composition/candidate methods are largely empty; Arlyon handles preedit and
committed text but explicitly leaves IMM-style candidate/clause APIs unavailable.
Neither source tree alone establishes complete native input behavior.

## How this shapes the first M2 changes

Proceed in reviewable work packages, retaining Windows regression checks:

1. Extend the M1 native ABI with filesystem/path services and synthetic hosted
   fixtures: creation dispositions, positioned/partial I/O, interrupted calls,
   EOF, Unicode names, no-replace rename, case ambiguity and read-only data with
   writable user settings/cache. Use separate Linux and Darwin leaves where the
   semantics differ. Do not replace the existing build/platform configuration.
2. Wire Storm/BlizzardCore package I/O and the existing
   [Services/AsyncFile.cpp](../../Engine/Source/Services/AsyncFile.cpp) callers.
   Both inspected forks retain that async service file unchanged. Test its
   callback thread/order, priority, cancellation/destruction and shutdown with
   the M1 services; nearby POSIX functions are not sufficient integration proof.
   Add MPQ patch/locale and failed-signature fixtures, including block-table
   expansion. Keep signature defaults enabled.
3. Implement the nearby-allocation contract before loading M2 model buffers.
   Validate representative DBC, BLP, M2, WMO and map decoding against separately
   identified unchanged 12340 data. Synthetic fixtures provide CI coverage but
   cannot certify the external data installation.
4. Add a pinned SDL3 shell with settings save/reload, a main-thread event pump,
   input/clipboard support, explicit optional-peripheral availability and normal
   shutdown checks. Extend Actions targets only as the subset becomes buildable;
   retain the full native-client gate until the actual client is supported.

DXVK Native/MoltenVK deserves a bounded comparison during the graphics decision:
it may preserve more existing D3D9 behavior than a new adapter. Arlyon's
[smoke documentation][arlyon-dxvk] pins a macOS-capable DXVK fork at `8703715` and
reports actual Apple hardware readback/presentation. This does not select it over
the planned Gx/bgfx prototype. Compare representative UI, terrain, skinning,
shader permutations and offscreen rendering before changing graphics direction.
The existing Windows OpenGL failure remains relevant to the planned Linux
compatibility renderer, but this review did not diagnose or fix it.

## Validation of this review

- Read both pinned trees and relevant local plan, baseline, M1 design and module
  CMake files; compared source files using the existing comment/token comparison
  helpers without the 15595-only CLI guard. That heuristic was a triage aid, not
  proof of semantic equivalence. Scratch comparisons remain outside the product.
- Checked the external Actions job results and Arlyon PR/report evidence above.
  Did not download or execute reference client binaries, run reference tests,
  configure/build a client or dependency, or inspect unrelated branch source.
- Checked local documentation links and `git diff --check`. Only this review and
  its plan cross-reference changed; historical M0/M1 evidence is preserved.
- Still outstanding: this project's M2 compilation, semantic fixtures, real data
  decoding, SDL desktop/hardware behavior and native application lifecycle.
  M0 clean-data/server/capture gaps remain unchanged.

[rose-tree]: https://github.com/RosemyneH/wowdecomp/tree/d91c854bc9c6af8d27e6e4093d76985671893bc5
[rose-readme]: https://github.com/RosemyneH/wowdecomp/blob/d91c854bc9c6af8d27e6e4093d76985671893bc5/README.md
[rose-linux-run]: https://github.com/RosemyneH/wowdecomp/actions/runs/38985039986
[rose-mac-run]: https://github.com/RosemyneH/wowdecomp/actions/runs/38985039808
[rose-windows-run]: https://github.com/RosemyneH/wowdecomp/actions/runs/38985039952
[rose-near]: https://github.com/RosemyneH/wowdecomp/blob/d91c854bc9c6af8d27e6e4093d76985671893bc5/Engine/Source/Os/Linux/OsMemory.cpp
[rose-readnative]: https://github.com/RosemyneH/wowdecomp/blob/d91c854bc9c6af8d27e6e4093d76985671893bc5/Engine/Source/BlizzardCore/Source/Packages/iounit/Linux/IOFileUnit.cpp#L13-L47
[rose-storm-read]: https://github.com/RosemyneH/wowdecomp/blob/d91c854bc9c6af8d27e6e4093d76985671893bc5/Engine/Source/Storm/Source/Linux/IOFileUnit.cpp#L182-L198
[rose-path]: https://github.com/RosemyneH/wowdecomp/blob/d91c854bc9c6af8d27e6e4093d76985671893bc5/Engine/Source/BlizzardCore/Source/System/File/Linux/Filestack.cpp#L61-L166
[rose-random]: https://github.com/RosemyneH/wowdecomp/blob/d91c854bc9c6af8d27e6e4093d76985671893bc5/Engine/Source/Os/Linux/OsSecureRandom.cpp
[arlyon-tree]: https://github.com/arlyon/wowdecomp/tree/24778cd77a6a217bbfdffccb88d9556a06b8b21d
[arlyon-bringup]: https://github.com/arlyon/wowdecomp/blob/24778cd77a6a217bbfdffccb88d9556a06b8b21d/docs/MacOSBringup.md
[arlyon-pr]: https://github.com/arlyon/wowdecomp/pull/6
[arlyon-quit]: https://github.com/arlyon/wowdecomp/blob/24778cd77a6a217bbfdffccb88d9556a06b8b21d/Tools/MacSmoke/README.md
[arlyon-sdl]: https://github.com/arlyon/wowdecomp/tree/24778cd77a6a217bbfdffccb88d9556a06b8b21d/Engine/Source/Os/SDL
[arlyon-mpq]: https://github.com/arlyon/wowdecomp/blob/24778cd77a6a217bbfdffccb88d9556a06b8b21d/Engine/Source/BlizzardCore/Source/Packages/Mopaq/SFileArchives.cpp#L1269-L1305
[arlyon-near]: https://github.com/arlyon/wowdecomp/blob/24778cd77a6a217bbfdffccb88d9556a06b8b21d/Engine/Source/Os/Mac/OsMemory.cpp
[arlyon-entry-build]: https://github.com/arlyon/wowdecomp/blob/24778cd77a6a217bbfdffccb88d9556a06b8b21d/WoW/Source/CMakeLists.txt
[arlyon-entry]: https://github.com/arlyon/wowdecomp/blob/24778cd77a6a217bbfdffccb88d9556a06b8b21d/WoW/Source/Mac/main.cpp
[arlyon-command]: https://github.com/arlyon/wowdecomp/blob/24778cd77a6a217bbfdffccb88d9556a06b8b21d/Engine/Source/Storm/Source/SCmd.cpp#L558-L561
[arlyon-net-build]: https://github.com/arlyon/wowdecomp/blob/24778cd77a6a217bbfdffccb88d9556a06b8b21d/WoW/Source/WowServices/CMakeLists.txt
[arlyon-dxvk]: https://github.com/arlyon/wowdecomp/blob/24778cd77a6a217bbfdffccb88d9556a06b8b21d/Tools/DxvkSmoke/README.md
