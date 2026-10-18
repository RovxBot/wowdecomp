# M2 handoff — updated 2026-10-18

M2 is complete for the observed native Linux asset/shell exit criteria. The
selected X11 profile loads the external 12340/enUS reference, shows original
terrain pixels, saves/reloads settings and exits with no worker threads or
ASan/UBSan/LeakSanitizer diagnostics. Operator Wayland input acceptance is also
recorded. Wayland's separate system Fontconfig report remains a qualification;
this is not a clean Wayland sanitizer claim. No M3 full-game work has started.

The [final closeout](closeout.json) retains implementation head
`50a7d91a1794529da6bc094fcf58dc5435392033`, merged in
[PR #11](https://github.com/RovxBot/wowdecomp/pull/11) as
`df0f2d0f1f7abf86afd125f9d4d42d37ce542625`, all five hosted manifests,
full Windows regression identity, downloaded hashes/types and exact-source runtime
results. [Desktop lifetime design](DESKTOP_LIFETIMES.md) explains the linkage
choice, required runtime libraries and remaining Wayland report.

| M2 exit requirement | Observed evidence |
| --- | --- |
| Usable native Linux window and original data | Actual X11 release/sanitizer startup; authenticated assets and displayed terrain pixels; separate Wayland operator input acceptance |
| Settings persistence | Two exact-source X11 sanitizer launches, verified save/reload, launch count 1 then 2 |
| Clean application shutdown | Release and sanitizer exit 0, empty stderr, zero workers; eight SDL-only init/quit cycles also clean |
| Retained cross-platform regression | Five shell configurations each passed 12 CTest checks, actual ARM64 execution, full Windows client compilation and all seven PR workflows passed |

Acceptance is explicitly the tested X11 Linux profile. The external Wayland
Fontconfig qualification and unobserved desktop/hardware combinations remain
visible below and in the coverage record; they are not relabeled as passing tests.

| Work package | Merged PR | Contract/evidence |
| --- | --- | --- |
| Host files and separate roots | [#5](https://github.com/RovxBot/wowdecomp/pull/5) | [FILESYSTEM.md](FILESYSTEM.md) |
| Authenticated MPQs and scalar assets | [#6](https://github.com/RovxBot/wowdecomp/pull/6) | [ASSETS.md](ASSETS.md) |
| Owner-thread async files | [#7](https://github.com/RovxBot/wowdecomp/pull/7) | [ASYNC.md](ASYNC.md) |
| SDL3 window/input and persistent settings | [#8](https://github.com/RovxBot/wowdecomp/pull/8) | [SHELL.md](SHELL.md) |

Initial integrated shell implementation: `2154bed3917ab1322545a365a58c5d2e708906b3`, merged as
`6eecb78e99c889e6f97e55ce9fe5d5855d95ab89`. The
[hosted build record](shell-builds.json) retains all four manifests, artifact IDs
and digests, binary/notice hashes, toolchains and architectures. Earlier reports
retain their historical checkpoint identities.

## Final validation

[Push run 39081876080](https://github.com/RovxBot/wowdecomp/actions/runs/39081876080)
and [PR run 39081878566](https://github.com/RovxBot/wowdecomp/actions/runs/39081878566)
passed Linux release, Linux ASan/UBSan, dynamic-loading diagnostic, macOS ARM64
and Windows x64 native ABI configurations. Each ran 12 CTest checks. Final full
Windows regression [39081878559](https://github.com/RovxBot/wowdecomp/actions/runs/39081878559)
also passed with signatures enabled. The closeout keeps the PR merge checkout's
source identity distinct from head `50a7d91` and the eventual main merge.

The exact-source default Linux release and sanitizer binaries were hash-verified
before execution. X11 assets/settings/shutdown passed; the sanitizer relaunch
preserved settings and incremented the launch count. The engine-free control
completed eight cycles with empty stderr. Final release Wayland startup/shutdown
passed; final Wayland sanitizer still reports 123,210 bytes / 3,266 Fontconfig
allocations, separately classified using the controls. All detectors stayed
enabled, without `LSAN_OPTIONS`, `LD_PRELOAD`, `LD_AUDIT` or an interposer.

## Historical validation checkpoints

- [Push matrix 39023953930](https://github.com/RovxBot/wowdecomp/actions/runs/39023953930)
  and [PR matrix 39023956052](https://github.com/RovxBot/wowdecomp/actions/runs/39023956052)
  passed all four configurations: Linux release, Linux ASan/UBSan, macOS ARM64 and
  Windows x64 native ABI. Each ran 11 CTest checks, including the entire preceding
  core/file/package/decoder/async subset, SDL dummy input and settings failure cases.
  macOS executed on ARM64 hardware and checked ARM64 executable identities.
- [Full Windows client regression 39023956082](https://github.com/RovxBot/wowdecomp/actions/runs/39023956082)
  passed with signature checks enabled. Its PR merge checkout is
  `03b789787bf4b930f467c68081eb33b63e88bd21`, distinct from the implementation head
  and eventual main merge. The [Windows record](shell-windows-regression.json)
  preserves that identity. This is compilation, not a new Windows gameplay run.
- [Final native asset execution](native-asset-runtime-shell.json) used the
  checksum-verified hosted sanitizer inspector against the unchanged external
  reference. It authenticated the original signaturefile (28,948 applicable
  records) and decoded DBC, BLP, MD20/SKIN, WMO, WDT and ADT. ASan/UBSan and leak
  detection remained enabled; exit 0, no diagnostics. The
  [clean-data inventory](clean-data-inventory.json) identifies the dataset.
- [Desktop runtime records](shell-runtime.json) distinguish X11, Wayland, release,
  sanitizer and settings-relaunch executions. Screenshots remain outside Git;
  visual inspection showed legible shell text and the decoded terrain texture.
  `assets=verified`, `settings-save-reload=verified` and
  `workers-after-shutdown=0` are required report results.
- [Injected X11 input](x11-injected-input.json) exercised ASCII/backspace, save,
  resize, focus, fullscreen and window close on Xwayland with SDL's core-input
  fallback. It did **not** establish physical XI2 input, relative motion, clipboard
  or IME. In particular, `relative-events=0` is not a passing mouse test.

The initial unsuppressed desktop sanitizer runs were **not passing**. Wayland with
the host's default libdecor GTK plugin reports 123,338 bytes in 3,267 allocations,
with direct allocations in fontconfig and GTK/Pango decoration paths. Selecting
the installed Cairo plugin for diagnosis still reports 131,629 bytes in 3,570
allocations. X11 with matching `EGL_PLATFORM=x11` reports 50,184 bytes in 914
allocations; some allocating modules were unloaded before reporting, with visible
SDL callers in X11 scaling and keyboard initialization. These were unattributed at that checkpoint. The later SDL-only comparison
reproduces them and establishes the clean linked X11 profile; the remaining
linked Wayland groups all allocate in Fontconfig. That report remains external
qualification, without a claim that the allocations are harmless.
[SDL's upstream guidance](https://wiki.libsdl.org/SDL3/HowToReportBugs#reporting-memory-leaks)
and [maintainer discussion](https://github.com/libsdl-org/SDL/issues/15244)
describe similar external/global-library reports. They do not prove the cause of
every allocation here. No suppression, disabled leak detection, global font-cache
cleanup or system-library replacement was added to manufacture a pass.

One initial X11 sanitizer run inherited this desktop's `EGL_PLATFORM=wayland` and
crashed inside Mesa/Wayland display setup. Setting the variable to `x11` allowed
startup and exposed the leak report above. [Mesa documents the platform
override](https://docs.mesa3d.org/egl.html#environment-variables).
The runtime record preserves both outcomes, and the release tests use matching
process-local EGL settings. No system environment was changed.

The launcher follow-up now matches child `EGL_PLATFORM` when Linux `--video` is
explicit, records effective graphics/input environment and writes a failed JSON
record for timeout (exit 124). It removes an earlier session report before launch
so failure cannot inherit a previous pass. Actual hosted-binary execution verified
X11 startup with the conflicting inherited override and an intentional one-second
timeout with a stale session file; the latter correctly failed and discarded that
stale file. [The runtime record](shell-runtime.json) preserves both checks.

All compilation occurred in GitHub Actions. Local work used source inspection,
Python source/document checks and execution of downloaded, hash-verified binaries.
No game installation, server policy or original Windows input implementation was
changed. No game assets or private user settings are committed.

The final documentation handoff passed `python -m unittest discover -s Tools/Porting
-p 'test_*.py'` locally: 16 discovered, 15 passed, one CMake selection check skipped
because CMake is unavailable. M2 JSON parsing, local Markdown file references and
`git diff --check` passed. Documentation-only changes require no client rebuild.

## Coverage qualifications and next work

The follow-up [desktop lifetime investigation](DESKTOP_LIFETIMES.md) reproduces
the X11 report in an SDL-only control and records clean X11 control/shell results
with normal ELF display-library ownership, including eight init/quit cycles and
settings relaunch. Linux defaults now use that linkage; the exact final source
passed the complete baseline verification recorded above. The
remaining Wayland Fontconfig report also reproduces without engine code or a
renderer and is retained as a platform-library qualification, not a passing leak
check. The historical failures above remain unchanged.

On 2026-10-18 the operator confirmed that all requested physical-input checks
passed without visible errors: typing/backspace, clipboard, IME if available,
F2/Escape capture, F11, and close/relaunch settings. The
[acceptance record](operator-acceptance.json) matches the final `2154bed` hosted
binary on Wayland and records exit 0, 1,022 relative events, three clipboard actions,
verified settings reload and zero workers after shutdown. The IME language/sample
was not identified, so this does not establish any particular language or wider
display/platform coverage. The earlier unattended timeout remains a historical
failed attempt, separately from this successful operator session. The native
window/input, shell and settings implementation bodies are unchanged between
that tested `2154bed` binary and final head `50a7d91`; their identities remain
distinct in the record.

The Linux desktop smoke tests do not establish macOS Cocoa desktop behavior,
Windows physical input, fractional/Retina or multiple-display coverage. ARM64
fixtures establish native execution of the allowlisted code, not the complete
macOS application milestone. Optional vendor hardware SDKs are visibly absent.

The shell uses a diagnostic SDL software renderer, with GLES2/Metal framebuffer
presentation where required. This is neither the Gx login/world path nor the
representative Vulkan/Metal graphics prototype. Scalar geometry inspection does
not implement complete animation, materials, FrameXML, addon loading, networking,
audio or movies. M0 Windows captures and live GrimGuzzler/Warden evidence remain
open independently.

M2 is closed for the acceptance profile above. M3 is the next implementation
milestone through existing Gx/network boundaries; it has not been started here.
Keep the full native client gate until its actual allowlist and dependencies work;
do not link native ABI objects into legacy Windows targets. Preserve original
12340 verification/protocol/data behavior, the model arena's signed-relative range
and ownership constraints, and owner-thread async completion. Review the
[native references](REFERENCE_REVIEW.md) and [backport ledger](../BACKPORTS.md)
before importing any fork/donor implementation. The shell does not select the
later graphics architecture by itself.
