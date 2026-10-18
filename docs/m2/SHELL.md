# M2 native SDL3 shell

Implemented and merged in [PR #8](https://github.com/RovxBot/wowdecomp/pull/8),
head `2154bed3917ab1322545a365a58c5d2e708906b3`. Hosted checks and actual Linux
desktop startup, original-data reads, settings reload and joined shutdown are
recorded in the [M2 handoff](HANDOFF.md), [build record](shell-builds.json) and
[runtime record](shell-runtime.json). The operator confirmed physical-input checks
passed on 2026-10-18; [the acceptance record](operator-acceptance.json) retains the
matching final Wayland session. Desktop LeakSanitizer triage remains open. The full
native game remains gated.

The shell uses BlizzardCore's authenticated packages and scalar asset decoders,
the Services owner-thread file queue and Os input/clipboard/IME entry points.
SDL owns the process main thread. Asset work runs on the verified 8 MiB worker;
the worker never touches SDL. Shutdown cancels pending reads, joins the worker,
drains callbacks, saves/reloads settings and releases SDL before core teardown.
Native and legacy Windows runtime ABIs remain separate.

## Dependency and input contracts

[SDL 3.2.28](https://github.com/libsdl-org/SDL/releases/tag/release-3.2.28) is pinned
to `7f3ae3d57459e59943a4ecfefc8f6277ec6bf540`, archive SHA256
`7a8347c770b90b33daac2352858ca03f9c9a2ccc8ce711054870361d1a6b32e5`.
Actions builds the static library and retains its license with the artifacts.
Audio and joystick/HID peripheral services are disabled for this diagnostic
target. The software renderer uses SDL's GLES2/Metal texture-framebuffer
presentation where Wayland/Cocoa require it. The texture preview establishes decoded pixels
and a usable shell, not a Gx backend, world renderer or graphics prototype.
NVAPI, SteelSeries and Logitech LCD are absent and explicitly reported optional.

SDL key/text events are translated to the existing Os event IDs and parameter
positions. Native enum sentinels retain their declared uint32 width; native Super
keys support Command shortcuts without changing legacy Windows key values.
Key/button state changes become visible in delivery order. Focus loss releases
held keys/buttons, composition and relative capture. Relative motion preserves
fractional deltas, restores the cursor anchor on normal release, and never edits
the existing Windows Input.cpp implementations. Wheel deltas use the existing
120-unit convention. Logical window dimensions and pixel dimensions stay distinct.

UTF-8 input is validated and decoded to scalar character events. Composition and
candidate events own their snapshots through event delivery, avoiding pointers in
32-bit OSEVENT parameters. IME commits use the existing result action and are not
also inserted as character events. SDL text-input areas are converted from the
render canvas to window coordinates for candidate placement. The OS owns input
language switching/native candidate UI; forced language selection/query and secure
capture are explicitly unavailable. The diagnostic font displays non-ASCII input
as `[U+...]`; original game font/FrameXML rendering remains later work.

`native-shell.settings` stores only shell dimensions, launch count and a UTF-8
text sample. Linux defaults use separate XDG config/cache roots; macOS defaults use
Application Support/Caches. Windows comparison requires explicit roots. The data
root is never writable settings storage. Save writes an exclusively created random
temporary file, handles short writes, flushes/closes it, then atomically renames
with replacement. Failure preserves the previous file and cleans up the temporary
file. This is separate from File::Move's existing no-replace contract. Persistence
of the directory entry across sudden power loss is not claimed.

## Build and execution

All compilation runs in [M2 shell Actions](../../.github/workflows/m2-shell.yml).
The `linux-window`, `linux-window-asan`, `macos-arm64-window` and `windows-window`
presets include core, file, package/decoder, async, SDL input and settings checks.
The input fixture uses SDL's dummy driver; it cannot establish desktop/physical IME
or GPU behavior. macOS checks require an ARM64 runner and ARM64 binaries.
`SDLDesktopControl` provides a separate engine-free lifetime control. Linux
display dependencies now use ordinary ELF linkage; the explicit
`linux-window-dynamic-asan` preset retains dynamic loading for comparison. See
[desktop library lifetimes](DESKTOP_LIFETIMES.md) for the required libraries,
clean X11 sanitizer results and remaining external Wayland Fontconfig report.

Download a passing `m2-linux-window-*` artifact from that workflow and extract it
outside the product/data tree. The runtime wrapper verifies `NativeShell` against
the hosted manifest before executing it; it does not configure or compile.
For the supplied Linux reference:

```sh
python Tools/Porting/run_native_shell.py \
  --artifact /tmp/extracted-shell-artifact \
  --data /mnt/games/3.3.5aClean/Data \
  --settings /tmp/wowdecomp-m2-settings --cache /tmp/wowdecomp-m2-cache \
  --output /tmp/wowdecomp-m2-desktop.json
```

Use an explicit `--video x11` or `--video wayland` to test that driver. Add
`--smoke-ms 1500 --snapshot /tmp/wowdecomp-m2-shell.bmp` for an automatic smoke run.
For the ASan artifact, add `--sanitizers`; leak checking needs execution outside a
ptrace debugger/sandbox. Real data and private session reports remain external.
On Linux, an explicit `--video x11` or `--video wayland` also matches
`EGL_PLATFORM` in the child process, avoiding an inherited conflicting override.
Direct binary execution must match these variables itself. The wrapper records
the effective settings, removes stale session output and records timeout as exit
124 with `timed_out=true`; that is a failed acceptance check. The desktop leak
reports and initial mismatched-platform crash remain in the handoff; a release
smoke pass does not establish leak-free execution.

## Desktop acceptance

1. Confirm the window reports successful original asset loading and displays the
   terrain texture. No login/world rendering is expected in this milestone.
2. Type ASCII/accented text and an IME sample; verify composition and a single
   committed result. Test backspace, key repeat, Ctrl/Command+C and V.
3. Resize, minimize/restore and change focus with keys/buttons held; confirm input
   is released. Check scaling on available displays.
4. Use F2 for relative mouse, move beyond the window edges and press Escape.
   Confirm cursor restoration, focus loss releases capture, and F11 toggles
   fullscreen. Record failures rather than assuming another driver also passed.
5. Save with Ctrl/Command+S, close using the window button, then relaunch with the
   same user roots. Confirm dimensions/text persist and the launch count increases.
   The report must show `workers-after-shutdown=0` and verified settings reload.

Record the tested artifact/source hash, OS/architecture, actual SDL driver,
display/session and observed results. Apple silicon desktop validation is distinct
from ARM64 fixture execution and from the full macOS game milestone. M0 server,
Warden and Windows world-scene evidence remain open independently.
