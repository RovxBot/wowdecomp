# Full Linux client target

This separate target adapts the complete 12340 Linux client from
[RosemyneH/wowdecomp](https://github.com/RosemyneH/wowdecomp), pinned by commit and
archive SHA256 in [upstream.json](upstream.json). It builds the full `WowClient`
graph, including world/gameplay, SDL3, direct Vulkan and native audio. Existing
Windows/macOS and diagnostic targets remain separate. Integration, comparison
and qualification are tracked in [#170](https://github.com/RovxBot/wowdecomp/issues/170).

## Build and download

All compilation runs in the
[Full Linux x64 application workflow](https://github.com/RovxBot/wowdecomp/actions/workflows/linux-x64.yml).
The matrix builds normal/TEST-SERVER release and strict ASan/UBSan/leak variants.
Choose `linux-x64-test-server-release-<sha>` for the extract-and-launch archive,
application and checksums;
`full-linux-evidence-*` separately retains manifests, adapted sources and logs.
Game data and credentials are never packaged. Distribution library versions and
hashes are recorded. The Ubuntu 26.04 runtime baseline requires glibc 2.43+;
the host supplies Python 3.10+, Vulkan GPU/ICD and a
working X11 or Wayland session. The AppImage runtime needs FUSE, or use
`APPIMAGE_EXTRACT_AND_RUN=1` for its built-in temporary extraction.

Public [releases](https://github.com/RovxBot/wowdecomp/releases) contain only the
AppImage and its SHA256 file. Place the AppImage in your WoW folder beside
`Data`, mark it executable if needed, and double-click it. Keep developer
evidence in Actions artifacts and tracking issues.

For the standalone AppImage, place it beside the original clean `Data` directory,
verify the checksum and enable execution:

```sh
sha256sum -c WoW-Linux-x86_64.AppImage.sha256
chmod +x WoW-Linux-x86_64.AppImage
./WoW-Linux-x86_64.AppImage --check
./WoW-Linux-x86_64.AppImage
```

For the separately labelled isolated-server application, after checking the
server prerequisites below:

```sh
chmod +x WoW-Linux-x86_64-TEST-SERVER.AppImage
./WoW-Linux-x86_64-TEST-SERVER.AppImage
```

`--data /path/to/WoW/Data` supports a separately located application. Launch is
independent of the working directory. Writable state lives under XDG config,
cache and state roots, separated by data path and build profile. On first launch,
original WTF preferences and account/character bindings and SavedVariables are
copied into external state. Original preferences take precedence once; any older
external Config.wtf is backed up as Config.before-import.wtf. Later Linux saves
persist and existing external account/character files win. Login credential
CVars, agreement/scanner approvals and Windows renderer selection are excluded;
agreement decisions remain interactive. The enUS realmlist supplies a missing endpoint.
Original addons are
available read-only; optional WASM addons are off. Failures and session metadata
are retained in the printed external log directory.

## Authentication and compatibility

Normal native sign-in remains unavailable until the existing executable-integrity,
ScanDLL/detector and applicable Warden requirements are met (#98). The separate
TEST-SERVER application enables interactive sign-in by default; no extra flag is
needed. Use it with an isolated server that already has `StrictVersionCheck=0`
and `Warden.Enabled=0`. The old `--test-server-login` flag remains a compatibility alias.
The client changes no server policy, claims no successful scan and disconnects
on unsupported Warden/bot-check requests. Sign-in and original agreement decisions
remain interactive. Reconnect and Battle.net authentication are unavailable.

The adaptation removes donor autologin/automatic agreement acceptance and its
injected options/addon-panel Lua. Original archive/interface signatures, original
InterfaceKey, ordered Glue creation and post-creation digest remain required.
Resolved native write paths are restricted to external state. Linux/x64 identity
is truthful. Donor `OFFSET` provenance is retained; adapted files lose donor
`VERIFY` claims until independently revalidated.

The donor includes broader Vulkan, threading and gameplay changes. Their presence
is not an exact Windows visual/protocol parity claim. The adapted TEST-SERVER
release has operator-observed character selection and gameplay. Broader physical
input, audio, strict hardware shutdown and macOS acceptance remain separate.
Use #170 and the existing acceptance issues for actual observed results.

Alpha 0.2.0 later-client interface work is tracked in
[#211](https://github.com/RovxBot/wowdecomp/issues/211). The first prerequisite
adds explicitly versioned, opt-in modern XML and native Lua composition to this full-client
target. It does not yet load complete Forever Edit Mode controls or move/save
gameplay frames. See the [compatibility contract and external-reference checks](../../docs/retail/COMPATIBILITY.md).

## Source preparation and checks

`prepare.py` verifies the upstream archive and applies explicit adaptations to a
separate source directory. `source-audit.json` records before/after file hashes;
the exact adapted source is retained with the application. Pinned downloads retry
transient transport failures up to three times without changing the URL or
checksum. Invalid bytes and permanent HTTP errors still fail immediately. Source preparation,
Python launcher tests and source audits may run locally. `build.py` refuses local
compilation. The Actions build uses upstream's full `WowClient` CMake target,
focused job-lifecycle, allocation, near-memory and audio-angle checks. Package
checks query compile/runtime gates in the actual login and Glue modules and start
the real executable without game data, retaining failures. Host distribution
libraries resolve first; only missing fallback SONAMEs enter the external session.
