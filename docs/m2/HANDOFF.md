# M2 handoff — 2026-10-17

M2 implementation is merged. Hosted checks pass, and the Linux shell loads the
external 12340/enUS reference, shows decoded original terrain pixels, saves/reloads
settings and shuts down its worker. Final sign-off remains pending physical input
acceptance and desktop LeakSanitizer triage. No M3 full-game work has started.

| Work package | Merged PR | Contract/evidence |
| --- | --- | --- |
| Host files and separate roots | [#5](https://github.com/RovxBot/wowdecomp/pull/5) | [FILESYSTEM.md](FILESYSTEM.md) |
| Authenticated MPQs and scalar assets | [#6](https://github.com/RovxBot/wowdecomp/pull/6) | [ASSETS.md](ASSETS.md) |
| Owner-thread async files | [#7](https://github.com/RovxBot/wowdecomp/pull/7) | [ASYNC.md](ASYNC.md) |
| SDL3 window/input and persistent settings | [#8](https://github.com/RovxBot/wowdecomp/pull/8) | [SHELL.md](SHELL.md) |

Final implementation: `2154bed3917ab1322545a365a58c5d2e708906b3`, merged as
`6eecb78e99c889e6f97e55ce9fe5d5855d95ab89`. The
[hosted build record](shell-builds.json) retains all four manifests, artifact IDs
and digests, binary/notice hashes, toolchains and architectures. Earlier reports
retain their historical checkpoint identities.

## Observed validation

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

The final unsuppressed desktop sanitizer runs are **not passing**. Wayland with
the host's default libdecor GTK plugin reports 123,338 bytes in 3,267 allocations,
with direct allocations in fontconfig and GTK/Pango decoration paths. Selecting
the installed Cairo plugin for diagnosis still reports 131,629 bytes in 3,570
allocations. X11 with matching `EGL_PLATFORM=x11` reports 50,184 bytes in 914
allocations; some allocating modules were unloaded before reporting, with visible
SDL callers in X11 scaling and keyboard initialization. These require further
attribution; they are not established as application-owned or dismissed as harmless.
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

The documentation handoff passed `python -m unittest discover -s Tools/Porting
-p 'test_*.py'` locally: 15 discovered, 14 passed, one CMake selection check skipped
because CMake is unavailable. M2 JSON parsing, local Markdown file references and
`git diff --check` passed. Documentation-only changes require no client rebuild.

## Remaining acceptance and next work

Use the runtime command and five-step [desktop checklist](SHELL.md#desktop-acceptance).
Record physical typing/backspace/repeat, copy/paste, IME composition/commit where
available, relative mouse/cursor restoration, focus loss, fullscreen, resize and
close/relaunch persistence. An opened test window or injected event count alone is
not operator confirmation. Record unavailable IME/display configurations explicitly.
The unattended interactive window timed out without a user report; it is not a
passing physical-input or clean-shutdown observation. Keep the milestone open
until both that acceptance and the desktop leak findings have a recorded disposition.

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

After M2 physical acceptance, continue M3 through existing Gx/network boundaries.
Keep the full native client gate until its actual allowlist and dependencies work;
do not link native ABI objects into legacy Windows targets. Preserve original
12340 verification/protocol/data behavior, the model arena's signed-relative range
and ownership constraints, and owner-thread async completion. Review the
[native references](REFERENCE_REVIEW.md) and [backport ledger](../BACKPORTS.md)
before importing any fork/donor implementation. The shell does not select the
later graphics architecture by itself.
