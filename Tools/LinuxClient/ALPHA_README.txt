WoW native Linux alpha 1 — TEST-SERVER

Extract the entire archive directly into your clean WoW 3.3.5a (12340) folder.
Data, the AppImage and Play WoW TEST-SERVER.desktop should be beside each other.
Double-click Play WoW TEST-SERVER.desktop. Your desktop may ask you to allow
launching this downloaded launcher once. Alternatively run the executable
Launch WoW-Linux-x86_64-TEST-SERVER.sh. No FUSE installation is needed.

Requires Linux x86-64, glibc 2.43+ (Ubuntu 26.04 or a sufficiently recent
distribution), Python 3.10+, Vulkan-capable graphics and a working desktop.
Clean enUS data is required and is not included. Other locales, older glibc,
NVIDIA/Intel graphics and native Wayland gameplay are not yet qualified.

Use an isolated compatible server that already has StrictVersionCheck=0 and
Warden.Enabled=0. Interactive sign-in is enabled in this labelled build.
Production native executable integrity/ScanDLL/Warden support is unavailable.
The application does not change server policy. Unsupported Warden/bot requests
disconnect. Agreements and credentials remain interactive.

On first launch the original WTF preferences, account/character bindings and
SavedVariables are copied to private external Linux state. The original folder
is never changed. Original login credential CVars, agreement/scanner approvals
and Windows renderer selections are excluded. An older Linux Config.wtf is
backed up as Config.before-import.wtf before this one-time migration. Original
preferences take precedence during that migration; later Linux saves persist.
Existing external account/character files win. Addons are read from Interface.

Settings: ~/.config/wowdecomp/full-linux/test-server/<installation-id>/
Cache: ~/.cache/wowdecomp/full-linux/test-server/<installation-id>/
Logs: ~/.local/state/wowdecomp/full-linux/test-server/<installation-id>/
XDG overrides are supported. The launcher prints the session log location.
For a separate application location, pass --data /path/to/WoW/Data.
--check verifies the application and prints paths without changing settings.

Alpha limitations: broader gameplay, addons, input, graphics parity, audio/device
recovery and strict original-data startup/shutdown remain under qualification.
This release is experimental. Read current issues before reporting a duplicate.
Do not attach credentials, full WTF files, private SavedVariables or game data.

Source, provenance, retained validation and issues:
https://github.com/RovxBot/wowdecomp
https://github.com/RovxBot/wowdecomp/issues/174
https://github.com/RovxBot/wowdecomp/issues/172
Embedded notices, dependency versions/hashes and adapted-source identities are
inside the AppImage; the release also supplies exact adapted source separately.
