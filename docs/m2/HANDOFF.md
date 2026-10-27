# Native assets and shell reference

Current tasks and validation updates belong in the
[issue tracker](https://github.com/RovxBot/wowdecomp/issues). This page is a contract/evidence index rather
than a milestone work log.

| Area | Contract | Retained evidence |
| --- | --- | --- |
| Host files and separate roots | [Filesystem](FILESYSTEM.md) | [File-service record](file-services.json) |
| Authenticated MPQs and scalar assets | [Assets](ASSETS.md) | [Clean-data inventory](clean-data-inventory.json), [asset runtime](native-asset-runtime-final.json) |
| Owner-thread async reads | [Async](ASYNC.md) | Contracts and fixtures linked in the guide |
| SDL window/input and settings | [Shell](SHELL.md) | [Hosted builds](shell-builds.json), [runtime](shell-runtime.json), [operator acceptance](operator-acceptance.json) |
| Desktop-library lifetime/shutdown | [Desktop lifetimes](DESKTOP_LIFETIMES.md) | [Closeout](closeout.json), [leak triage](desktop-leak-triage.json) |
| Imported/native references | [Reference review](REFERENCE_REVIEW.md) | [Backport ledger](../BACKPORTS.md) |

The recorded M2 closeout qualifies the observed Linux X11 asset/shell baseline.
Earlier Wayland shell operator input acceptance remains separate and did not
identify a specific IME language/sample. The reproduced external Wayland
Fontconfig report is tracked in [#108](https://github.com/RovxBot/wowdecomp/issues/108); it is not a clean
Wayland LeakSanitizer pass. Wider desktop/hardware input coverage is not implied.

Keep both data installations read-only with settings/cache/logs external. The
diagnostic shell's software presentation and scalar geometry are separate from
original Gx/model/world rendering. ARM64 component fixtures do not establish
a macOS game application. All compilation remains in Actions.

The obsolete chronological handoff is retained in
[pinned history](https://github.com/RovxBot/wowdecomp/blob/6a18bde50e300fc34d7ed983a2ed1dc97ef76d58/docs/m2/HANDOFF.md). Its exact evidence
manifests remain in the repository; use the live issues for next steps.
