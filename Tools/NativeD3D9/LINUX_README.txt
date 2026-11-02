WoW Native for Linux x86-64 (3.3.5a / 12340 enUS)

Extract this tar.gz into your clean WoW folder. Keep the WoW-Native-Linux
subfolder beside the original Data folder, then double-click Play WoW.desktop
or Launch WoW.sh inside it. No source checkout or compilation is needed.
Some file managers require Allow Launching / Trust / Run as a program first.
The tar.gz preserves executable permissions; the Actions diagnostic ZIP does not.
You can also run: sh "Launch WoW.sh"

Requirements: Linux x86-64 with glibc 2.39+ (Ubuntu 24.04 or equivalent),
Python 3.10+, X11 or Xwayland, and a Vulkan 1.3 GPU driver supporting DXVK 2.7.1.
SDL3, DXVK, the Vulkan loader, C++ runtime and linked display libraries are
bundled with notices. glibc, GPU drivers/ICDs, Python and a terminal emulator
come from your distribution. An incompatible/missing GPU remains an error.

Settings: $XDG_CONFIG_HOME/wowdecomp/<profile>/<installation-id>/WTF/Config.wtf
Cache:    $XDG_CACHE_HOME/wowdecomp/<profile>/<installation-id>/
Logs:     $XDG_STATE_HOME/wowdecomp/<profile>/<installation-id>/<session>/
Defaults: ~/.config, ~/.cache, ~/.local/state respectively.
Release, TEST-SERVER and strict profiles have separate state. Paths use the
resolved Data directory so moving/upgrading the native package preserves state.

The launcher reads Data/enUS/realmlist.wtf (or the installation's root
realmlist.wtf when the locale file is absent) once to seed only SET realmlist
in external settings. An existing external SET realmlist always wins, including
an empty value. Edit the external Config.wtf to change the endpoint afterwards.
It never copies original account settings or accepted agreements. Original
agreements require your interactive decisions. Original Data, MPQs, Lua/XML,
signatures and the installation's WTF/Cache/Logs are never changed.

The client is still in development: normal builds refuse sign-in until native
integrity requirements are implemented. Packaging does not enable gameplay.
TEST-SERVER packages are separately compiled and labelled. Their ordinary
Play WoW launcher leaves test login disabled. Only explicitly choosing
Play WoW TEST-SERVER.desktop / Launch TEST-SERVER.sh, or passing
--test-server-login, activates isolated auth/realm-list testing. This requires
an externally configured isolated server; integrity and Warden are unavailable.
It stops after SRP verification and realm-list parsing; no character/world entry.
Never use this profile with a production endpoint. Strict packages keep all
ASan/UBSan/leak detectors enabled; driver leak failures remain failures.

Diagnostics (run from this package):
  python3 launch.py --check
  python3 launch.py --data /path/to/WoW/Data
  python3 launch.py --settings /external/settings --cache /external/cache --logs /external/logs
  python3 run_native_client.py --renderer dxvk --artifact . --data /path/to/WoW/Data --settings /external/settings --cache /external/cache --output /external/report.json --frames 300 --hidden
Use --sanitizers with the last command for a matching strict package. Interactive
launch has no diagnostic timeout or automatic screenshot. Each session retains
client.log, DXVK logs and source/exit identity in session.json outside the game.

Source, current limitations and support: https://github.com/RovxBot/wowdecomp
Launcher tracking: https://github.com/RovxBot/wowdecomp/issues/164
