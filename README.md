# wowdecomp

Bringing the World of Warcraft **3.3.5a client (build 12340)** to **Linux x86-64**
and **macOS ARM64**, with Windows x64 retained as the reference client.

**The full Linux client now has operator-observed character selection and gameplay
in its explicit TEST-SERVER profile.** It adapts the pinned RosemyneH client with
SDL3, direct Vulkan and external writable state, and ships as one experimental
AppImage. [Build/run instructions](Tools/LinuxClient/README.md) and
[qualification/gaps](https://github.com/RovxBot/wowdecomp/issues/170).
Normal native sign-in remains blocked pending executable-integrity and Warden
compatibility; this is not a production-integrity-qualified release.
The existing original D3D9/DXVK diagnostic target remains available separately.

This is a C++ reconstruction based on
[SatyPardus/wowdecomp](https://github.com/SatyPardus/wowdecomp), using the original
client code and Lua/XML interfaces. The compatibility target is unchanged 3.3.5a
data, protocol, gameplay, Lua APIs and addons. **Game data is not included; bring
your own clean installation.** The
[4.3.4 reconstruction](https://github.com/RovxBot/wowdecomp15595) supplies selected
implementation references, without changing the 12340 baseline.

[Try the Linux build](#getting-builds-and-running-diagnostics) ·
[Known gaps](#what-does-not-work-yet) ·
[Help test](#what-to-test-and-how-to-report-it) ·
[Current issues](https://github.com/RovxBot/wowdecomp/issues) ·
[Community Discord](https://discord.gg/DsZMGUD8aT)

## Where we are

Status reviewed **3 November 2026**. Issues hold the current acceptance criteria
and detailed evidence; successful builds and component tests cover only their
stated scope.

Alpha 0.2.0 work now targets [genuine Forever/retail feature backports](https://github.com/RovxBot/wowdecomp/issues/211).
The first [native XML compatibility prerequisite](docs/retail/COMPATIBILITY.md)
does not yet provide complete Edit Mode controls or gameplay-frame layouts.

| Platform | Demonstrated progress | Available testing |
| --- | --- | --- |
| **Linux x86-64** | Full direct-Vulkan client: operator-observed original UI, character selection and gameplay on the isolated server. Existing D3D9/DXVK target: signed login rendering and component diagnostics. | [Full-client AppImage](Tools/LinuxClient/README.md): the labelled TEST-SERVER application enables interactive sign-in by default. Older DXVK packages are below. Broader play/input/audio/strict acceptance remains open. |
| **macOS ARM64 / Apple silicon** | Actual ARM64 execution of core, file/asset, packet, networking, Lua, XML and layout component tests in Actions | Component artifacts. An integrated native game application and renderer are still pending [#102](https://github.com/RovxBot/wowdecomp/issues/102). |
| **Windows x64** | Full client compilation in Actions; operator-reported D3D9 runtime passes | Reference client. Reproducible scene captures and a complete recorded runtime baseline remain open in [#107](https://github.com/RovxBot/wowdecomp/issues/107). |

### Full Linux application

The separate full-client source graph includes original character/world callers,
SDL3 Linux services, direct Vulkan and native audio. Operator sign-in has reached
character selection and gameplay with unchanged clean 12340/enUS data and required
signatures. Download the [Linux AppImage](https://github.com/RovxBot/wowdecomp/releases),
place it in your clean WoW folder beside `Data`, mark it executable if needed,
and double-click it. This experimental build requires glibc 2.43+
(Ubuntu 26.04 or a sufficiently recent distribution), Python 3.10+ and Vulkan.
The labelled build enables interactive sign-in for compatible isolated servers;
normal production integrity remains unavailable. Original WTF preferences are
imported once into external Linux state, with subsequent Linux saves preserved.
See [usage and limitations](Tools/LinuxClient/ALPHA_README.txt).
Fresh development applications are also available from
[Full Linux x64 application — Actions](https://github.com/RovxBot/wowdecomp/actions/workflows/linux-x64.yml).
The [integration issue](https://github.com/RovxBot/wowdecomp/issues/170) distinguishes
that observed release result from unqualified behavior and strict checks.

### Existing DXVK diagnostic target

- **Original-data startup:** authenticated MPQ reads, all 60 ordered original
  GlueXML visits, mandatory post-creation digest verification and login activation.
  Original Lua/XML and signature checks stay in use.
- **Original login rendering:** Wrath/Blizzard logos, textured controls and
  agreements, Northrend architecture, animated dragon, moving sky/snow, original
  model skinning, particles and post-processing. These have been captured on an
  RX 7900 XTX with RADV 26.2.5 using clean 12340/enUS data.
  [Rendering evidence](https://github.com/RovxBot/wowdecomp/issues/133#issuecomment-6554949208).
- **Some physical UI interaction:** notice dismissal, field typing and repeated
  opening/closing of the main Options menu have been observed. Full control and
  input acceptance remains open in [#96](https://github.com/RovxBot/wowdecomp/issues/96)
  and [#106](https://github.com/RovxBot/wowdecomp/issues/106).
- **Desktop launch and separate writable state:** the package bundles its native
  runtime dependencies, verifies hashes, and keeps settings, caches and logs
  outside the game installation. Release launcher execution has reached original
  startup and exited cleanly with no remaining engine workers.
  [Package evidence](https://github.com/RovxBot/wowdecomp/issues/164#issuecomment-6568107608).
- **Networking in controlled tests:** original socket, Grunt/SRP authentication
  and realm-list code runs against test peers, including failure/cancellation
  cases. Interactive server authentication remains an outstanding test in
  [#98](https://github.com/RovxBot/wowdecomp/issues/98).

## What does not work yet

| Area | Current limitation |
| --- | --- |
| Normal native sign-in | **Disabled.** Native executable-integrity, detector and applicable Warden requirements are incomplete. Normal packages refuse credentials. [#98](https://github.com/RovxBot/wowdecomp/issues/98), [integrity contract](docs/m3/INTEGRITY.md) |
| Character selection and world entry | Operator-observed in the **full Linux TEST-SERVER application**; broader gameplay and strict coverage remain open in [#170](https://github.com/RovxBot/wowdecomp/issues/170)/[#100](https://github.com/RovxBot/wowdecomp/issues/100). The existing bounded DXVK target remains unqualified for these paths. |
| Some UI controls | Agreement-scrollbar dragging and opening an Options category have recorded callback failures. The main Options menu working does not establish its subpages. [#130](https://github.com/RovxBot/wowdecomp/issues/130), [#158](https://github.com/RovxBot/wowdecomp/issues/158) |
| Applied graphics settings | Saving a preference does not prove the renderer applied it. Gamma, terrain and other options still need implementation/validation. [#105](https://github.com/RovxBot/wowdecomp/issues/105) |
| Native sound and movies | Full-client implementations are included but playback/lifetime acceptance remains open. The existing DXVK diagnostic target has unavailable playback. [#103](https://github.com/RovxBot/wowdecomp/issues/103), [#104](https://github.com/RovxBot/wowdecomp/issues/104) |
| Strict shutdown on the tested RADV driver | Engine shutdown completes, then LeakSanitizer reports a **128-byte driver leak** on RADV 26.2.5. This remains a failed strict hardware check, separate from passing hosted tests. [#147](https://github.com/RovxBot/wowdecomp/issues/147) |
| External DNS shutdown | An active system DNS lookup can delay shutdown; bounded cancellation remains open. [#97](https://github.com/RovxBot/wowdecomp/issues/97) |

### What still needs testing

These are validation gaps, not claims that every case is broken:

- **Broader Linux hardware:** current original-scene evidence is from the AMD
  setup above. NVIDIA/Intel GPUs, other drivers/distributions and device-loss
  recovery need recorded results.
- **Full desktop input:** keyboard navigation, clipboard, focus changes, resize,
  fullscreen, DPI/multiple displays and real IME composition/candidates need a
  complete original-UI test matrix. The launcher uses X11/Xwayland; native Wayland
  game behavior is not qualified.
- **Visual and performance parity:** matching Windows D3D9 captures, representative
  terrain/interior/character scenes, frame times and extended sessions are pending.
  Login-scene rendering does not qualify world rendering.
- **Data and gameplay compatibility:** integrated native startup currently admits
  **12340/enUS**. Other locales, modified installations, live server sessions,
  addons and sustained gameplay have not been qualified by these tests.
- **Apple silicon game execution:** ARM64 component tests do not establish the
  original macOS application, graphics, desktop input or gameplay.

## Getting builds and running diagnostics

The separate [full Linux client target](Tools/LinuxClient/README.md) adapts the
pinned RosemyneH client and produces one AppImage through
[Full Linux x64 application — Actions](https://github.com/RovxBot/wowdecomp/actions/workflows/linux-x64.yml).
Its integration and runtime qualification remain tracked in
[#170](https://github.com/RovxBot/wowdecomp/issues/170). The labelled TEST-SERVER
AppImage enables interactive sign-in without an extra flag. The instructions below
describe the existing DXVK diagnostic/desktop target.

**All client and dependency compilation runs in GitHub Actions.** No local compiler
or source checkout is needed for the Linux desktop package. These are experimental
builds, with artifacts retained for 14 days.

### 1. Check the Linux requirements

- Linux **x86-64**, glibc **2.39+** (Ubuntu 24.04 baseline or equivalent).
- Python **3.10+**, an X11 session or Xwayland, and a terminal emulator.
- A host GPU/driver supporting **Vulkan 1.3 and DXVK 2.7.1**.
- An unchanged, clean **WoW 3.3.5a / 12340 / enUS** `Data` directory.

SDL3, DXVK, the Vulkan loader, C++ runtime and display-library fallbacks are
bundled with notices. Your distribution supplies glibc, Python, the terminal
and GPU drivers. Installed system libraries take precedence over bundled
distribution fallbacks so the host GPU driver can use its dependencies.

### 2. Download and verify a desktop package

Open [Original Linux D3D9 client — Actions](https://github.com/RovxBot/wowdecomp/actions/workflows/native-d3d9-client.yml).
Choose a successful run for the branch/revision you intend to test and download
**`linux-desktop-d3d9-sanitizers-false-<sha>`** for a normal release-mode test.
GitHub artifact downloads require a signed-in account.

For strict ASan/UBSan/leak testing, choose the matching
**`linux-desktop-d3d9-sanitizers-true-<sha>`** artifact. Names containing
`TEST-SERVER` identify the separate profile below. The `native-client-d3d9-*`
artifacts are diagnostic packages; choose `linux-desktop-d3d9-*` for desktop setup.

Compare the downloaded ZIP's SHA256 with the artifact `digest` from the Actions
API. With the GitHub CLI, replace `RUN_ID` with the selected run's number:

```sh
gh api repos/RovxBot/wowdecomp/actions/runs/RUN_ID/artifacts \
  --jq '.artifacts[] | {name, digest, expired}'
sha256sum /path/to/downloaded-artifact.zip
```

Extract the ZIP, then verify its enclosed tarball before unpacking it. For the
normal release package, run from the extracted artifact directory:

```sh
sha256sum -c WoW-Native-Linux.tar.gz.sha256
mkdir -p /path/to/writable-test-directory
tar -xzf WoW-Native-Linux.tar.gz -C /path/to/writable-test-directory
```

Strict/test packages use their corresponding tarball names. The tarball preserves
executable permissions; the launcher also checks packaged files against
`manifest.json`. Record the run, source revision and artifact name: a PR artifact
can identify a merge checkout rather than the branch head.

### 3. Launch against your original data

From the extracted `WoW-Native-Linux` directory:

```sh
python3 launch.py --check --data /path/to/WoW/Data
python3 launch.py --data /path/to/WoW/Data
```

`--check` verifies the package and prints paths without starting the client or
writing settings. The second command starts an interactive session without a
frame limit or diagnostic timeout.

For desktop discovery, the package folder can instead sit beside `Data`:

```text
WoW/
├── Data/
└── WoW-Native-Linux/
    ├── Play WoW.desktop
    ├── Launch WoW.sh
    └── ...
```

Double-click `Play WoW.desktop` or `Launch WoW.sh`, or run `sh "Launch WoW.sh"`
from the package directory. Some file managers require **Allow Launching**,
**Trust** or **Run as a program**. Physical desktop trust/double-click behavior
still needs broader operator testing. For read-only installations, keep the
package elsewhere and use the explicit `--data` command above.

### 4. Know what to expect

The client should verify original data and create the original agreements/login
interface with the animated Wrath scene. Make original agreement decisions
interactively. A native integrity/ScanDLL capability notice is expected;
dismissing it allows the controls to appear. **Normal builds still refuse Login.**
This is the current capability boundary.

Test rendering and controls, then close the window. Record missing artwork,
callback errors, crashes and shutdown failures. The known control/driver failures
above remain open.

The launcher prints external settings and session-log paths. Defaults are:

| Content | Default root |
| --- | --- |
| Settings, including `WTF/Config.wtf` | `~/.config/wowdecomp/` |
| Cache | `~/.cache/wowdecomp/` |
| Session logs/reports | `~/.local/state/wowdecomp/` |

XDG overrides are respected. State is separated by installation and
release/test/strict profile. Each session retains `client.log`, DXVK logs and
`session.json`. Only the existing realmlist endpoint is seeded into external
settings; existing external settings win. Original accounts/agreement decisions
are not copied. Keep all writable state outside the game installation.

### 5. Capture a bounded diagnostic run

From a verified package, use separate external paths for a reproducible startup
check and JSON report:

```sh
python3 run_native_client.py --renderer dxvk --artifact . \
  --data /path/to/WoW/Data \
  --settings /tmp/wowdecomp-check/settings \
  --cache /tmp/wowdecomp-check/cache \
  --output /tmp/wowdecomp-check/startup.json \
  --frames 300 --hidden --timeout 180
```

Add `--sanitizers` **only with the matching strict package**, using separate paths.
Keep ASan/UBSan/leak detection enabled and report nonzero exits and timeouts. A
300-frame diagnostic can remain at the agreement screen; it does not establish
physical input, authentication or gameplay.

More configuration: [Linux launcher and DXVK guide](docs/m3/DXVK_NATIVE.md#linux-desktop-package).

## Existing DXVK diagnostic test-server profile

This section describes the older D3D9/DXVK diagnostic profile. For the complete
Linux application with character selection/gameplay and sign-in enabled by
default in its labelled TEST-SERVER build, use the [full-client instructions](Tools/LinuxClient/README.md).
The diagnostic profile below connects original Grunt/SRP and realm-list parsing;
its observed result stops at authentication and realms, before character selection.

Use your own isolated 12340 test server, or one whose operator has explicitly
authorized your testing. The endpoint must already have
`StrictVersionCheck=0`, `Warden.Enabled=0` and accepts the truthful `Lnx`/`x64`
identity. Native integrity/Warden remain unavailable. The profile uses a documented
unavailable-checksum placeholder, not a successful executable-integrity check.
It is unsuitable for production servers.
[Complete profile contract](docs/m3/AUTHENTICATION.md#isolated-test-server-profile).

1. Download a verified **`linux-desktop-d3d9-TEST-SERVER-*`** artifact from a run
   explicitly built with `test_server_login=true`.
2. Run `python3 launch.py --check --data /path/to/WoW/Data` inside that package
   to locate its separate external settings root. Create/edit that root's
   `WTF/Config.wtf` with the intended endpoint:

   ```text
   SET realmlist "<isolated-host>:3724"
   ```

3. Launch with `python3 launch.py --data /path/to/WoW/Data --test-server-login`,
   or use `Launch TEST-SERVER.sh` / `Play WoW TEST-SERVER.desktop` when the
   package sits beside `Data`. Accept agreements and dismiss the notice
   interactively, then enter your test account yourself.
4. Record the visible result, received realm count, cancellation/error behavior
   and shutdown outcome in [#98](https://github.com/RovxBot/wowdecomp/issues/98).
   Keep credentials and session contents private.

The ordinary launcher in a test-capable package leaves login disabled. Normal
packages exclude the allowance and reject `--test-server-login`.

## What to test and how to report it

Useful contributions now are rendering/input reports from verified Linux
packages, strict shutdown results, isolated auth observations and matching
Windows D3D9 captures. Use the existing issue for a known problem; search before
opening a new one.

| Test | Where to report |
| --- | --- |
| Original login artwork, animation, text and controls | [#96](https://github.com/RovxBot/wowdecomp/issues/96), renderer [#133](https://github.com/RovxBot/wowdecomp/issues/133) |
| Typing, clipboard, keyboard focus and real IME | [#106](https://github.com/RovxBot/wowdecomp/issues/106) |
| Installation, path discovery, launcher and external state | [#164](https://github.com/RovxBot/wowdecomp/issues/164) |
| Explicit TEST-SERVER auth/realm-list behavior | [#98](https://github.com/RovxBot/wowdecomp/issues/98) |
| RADV strict shutdown leak | [#147](https://github.com/RovxBot/wowdecomp/issues/147) |
| Windows D3D9 reference scenes | [#107](https://github.com/RovxBot/wowdecomp/issues/107) |

Include the **Actions run/job link, artifact name, source revision and hashes**;
OS/architecture, GPU/driver and X11/Xwayland session; data build/locale; exact
launch command; steps and expected/actual behavior; exit code and relevant logs
or captures. State whether input was physical or automated and whether the build
was release, strict or TEST-SERVER. For auth, identify the actual running test
server build/policy without account details. Review attachments for credentials
and session content; avoid uploading game data or whole settings files.

### Requesting builds and testing other targets

Maintainers with Actions access can request fresh normal release/strict packages:

```sh
gh workflow run native-d3d9-client.yml --repo RovxBot/wowdecomp \
  --ref main -f configuration=both
```

For the isolated profile, explicitly add `-f test_server_login=true`. Use the
reviewed branch name instead of `main` for pending work and report its exact source.

[Windows baseline](https://github.com/RovxBot/wowdecomp/actions/workflows/windows-baseline.yml)
produces `windows-x64-baseline-*` artifacts. On Windows 10/11, stage the verified
`WowClient64.exe` / `Wow64.exe`, companion `XvidDecoder.dll` and a separate matching
`Data` copy outside the repository/reference installations; supply the required
Visual C++ runtime and configure the staging realmlist for your intended server.
`WowClient64.exe` includes the console/crash logger.
[Windows runtime record and limits](docs/m0/RUNTIME_SESSION.md).

For developers, [Actions](https://github.com/RovxBot/wowdecomp/actions) also provides
core, files/assets/async, packet/network, Lua/FrameScript/XML/layout and SDL shell
component workflows. Follow each workflow's module guide and manifest; these
artifacts are not interchangeable with a game package. The
[standalone DXVK prototype](docs/m3/DXVK_NATIVE.md#standalone-prototype) tests
device creation/drawing/resize without game data. Use the
[build scheduling policy](docs/CI_BUILD_POLICY.md) to choose focused checks.

## What we are working toward

The immediate path is complete original login/input → observed native
authentication → character selection → world entry and basic play. Audio, movies,
applied settings, platform coverage and repeated clean shutdown remain part of
the finished client. See [the porting plan](PORTING_PLAN.md) and
[active issues](https://github.com/RovxBot/wowdecomp/issues?q=is%3Aissue+is%3Aopen).

Current Linux rendering uses **original D3D9 → DXVK Native → Vulkan**, without Wine.
The longer-term renderer investigation starts with **SDL3 GPU Vulkan/Metal**,
compared with bgfx and reviewed alternatives under
[#101](https://github.com/RovxBot/wowdecomp/issues/101) and
[#134](https://github.com/RovxBot/wowdecomp/issues/134).
The older OpenGL adapter and its captures are frozen historical references;
new rendering work does not extend or fall back to it.

Enhanced weather, lighting, water, storms, presets and HD assets are planned
separately in issues #135–#140. They are not working features of the current
package. Enhancements remain optional; unchanged 12340 data stays the baseline.
Classic describes a visual preset, not an OpenGL backend.

## Source and contributing

| Path | Contents |
| --- | --- |
| `Engine/Source/` | Engine, platform services, rendering, assets, Frame/UI, sound and Lua |
| `WoW/Source/`, `WoW/Common/` | Original game, world, network and Glue code |
| `ThirdParty/` | Dependencies and notices |
| `Tools/NativeClient/`, `Tools/NativeD3D9/` | Integrated native startup, DXVK tooling and packaging |
| `Tools/Porting/` | Build tooling, source audits, launchers and validation |
| `Tools/WowClient/`, `Tools/XvidDecoder/` | Windows reference target/crash logger and x64 movie decoder |
| `docs/` | Architecture, module contracts and retained evidence |

Read [AGENTS.md](AGENTS.md), [PORTING_PLAN.md](PORTING_PLAN.md) and the relevant
issue/module guide before changing code. Keep compilation in Actions, preserve
Windows regression checks and native sanitizers, and leave game data read-only
with settings/cache/logs external. Preserve original signature checks, agreement
decisions, protocol and server policy. Test sign-in remains interactive.

The source follows the original module layout. `// OFFSET:` tags record original
binary addresses for reconstruction/provenance; coverage is not a measure of
native runtime readiness. Record imports and attribution in
[the backport ledger](docs/BACKPORTS.md). Current findings and validation belong
in issues; module documentation holds lasting contracts and instructions.

## Credits

- [SatyPardus/wowdecomp](https://github.com/SatyPardus/wowdecomp) and its contributors,
  for the original 3.3.5a reconstruction.
- SatyPardus and the [wowdecomp15595](https://github.com/RovxBot/wowdecomp15595)
  contributors, for the 4.3.4 reference.
- [arlyon/wowdecomp](https://github.com/arlyon/wowdecomp) and
  [RosemyneH/wowdecomp](https://github.com/RosemyneH/wowdecomp), for native-port
  references reviewed in [the reference audit](docs/m2/REFERENCE_REVIEW.md).
- [binana](https://github.com/thunderbrewhq/binana),
  [thunderbrew](https://github.com/thunderbrewhq/thunderbrew) and
  [whoa](https://github.com/whoahq/whoa).
- TOMRUS, for the initial IDB.
- The dependency authors, WoW modding community, contributors and everyone testing
  builds and reporting bugs.
