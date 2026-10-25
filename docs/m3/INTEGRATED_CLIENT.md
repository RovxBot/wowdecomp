# Integrated Linux startup

The opt-in `WOWDECOMP_BUILD_NATIVE_CLIENT` target `WowNativeClient` and
`linux-client` / `linux-client-asan` presets connect qualified native archives,
retained reads, SDL/Gx/Gxu, original FrameScript/XML/layout/Frame and images.
All compilation runs in Actions; Windows remains a separate regression target.

This is [PR #84](https://github.com/RovxBot/wowdecomp/pull/84), integration work in
progress. It does not yet execute `CommonMain`, complete `InitializeEngineCallback`
or full `CGlueMgr::Resume`, authenticate or enter the world. The entry adapts their
startup dependencies and original Glue interface-creation gate. Actual execution
must establish complete login rendering; fixture/window success does not.

Storm, floating point, an 8 MiB main stack and character conversion precede SDL
and workers. The C numeric locale remains intact. Explicit settings/cache roots
stay outside both read-only installations. Original CVars use original `SET`
syntax in external `WTF/Config.wtf`. Native files admit `OS_FILE_TYPE_ANY`;
other extended types remain unavailable. Native CVar reads bound sizes/short reads.

The Event adapter retains original IDs, priority/equal-priority ordering,
marker-based listener mutation and stop propagation on the SDL owner thread.
Active dispatch cannot destroy its context. No network scheduler is installed.
Input retains the existing SDL focus/IME/capture/lifetime contracts.

The ordered TOC/Include/Script/Font/template/frame path feeds actual original read
bytes into the creation digest. Read/script/factory errors, cycles and excessive
lines fail visibly and unwind trees/recursion state. Original InterfaceKey checks
precede creation; the full post-creation digest comparison remains mandatory.
Protected Lua errors are observed without replacing their handler. There are no
skipped widgets, modified original Lua/XML, alternate keys or unchecked modes.
SimpleHTML/hyperlink/ScrollFrame/Slider form the first startup-driven source batch.
Aligned pools retain matching deletion; HTML text closes before private fonts.

Close frames/callbacks before widget metatables, Scripts/templates, images,
renderer and Fonts/Gxu/layout/device/context. CVar strings close before XML's
string manager, then XML/VM, retained queue/packages and Storm/time. Existing
ownership guards remain authoritative. Authentication, server/Warden policy and
interactive sign-in remain unchanged.

## Downloaded execution

Verify an Actions archive digest and extract outside both reference installations:

```sh
python3 Tools/Porting/run_native_client.py \
  --artifact /tmp/extracted-native-linux-client \
  --data /mnt/games/3.3.5aClean/Data \
  --settings /tmp/waystone-settings --cache /tmp/waystone-cache \
  --frames 3 --output /tmp/waystone-startup.json
```

Add `--sanitizers` for the matching strict package. Omit `--frames` for a session
closed through window controls once startup completes. The launcher checks ELF/
binary/notice hashes and records identities, stages, errors and timeouts. It pins
X11/EGL-X11 and preserves ASan/UBSan/leak checks. No credentials are supplied.

Initial execution authenticated 28,948 archive records and 60 GlueXML visits,
created the original root and stopped at unavailable SimpleHTML in `GlueDialog`.
No original login frame was presented. Three initial strict desktop runs also
reported a 33-byte Xlib title-conversion leak before character-locale initialization;
these remain failed evidence. Subsequent compile/CVar-contract failures and exact
source/build/runtime identities are recorded separately as integration proceeds.
