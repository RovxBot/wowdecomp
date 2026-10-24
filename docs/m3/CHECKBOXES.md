# Original CheckButton widgets

The native Frame graph selects original 12340 `CSimpleCheckbox` through pooled
`CreateFrame('CheckButton', ...)`, XML and C++ factories. It retains original
checked/disabled-check texture selection, click registration and inherited Button
scripts/state. Original Windows factory code moves to the shared factory file;
its body and Windows widget/script bodies remain unchanged.

All widget state belongs to the Frame owner. State changes may update draw lists
during input callbacks but reject during batch collection. A native click checks
Button recursion/disabled admission before toggling the derived state. Textures
must occupy only one independent Button/CheckButton slot, including assignments
through inherited Button setters. Lua secondary texture arguments require
canonical live tables before derived-pointer use. Filename setters use the
qualified aligned region pool and unwind incomplete allocations.

Close widgets before Scripts/images/renderer/Fonts and lower owners. The Fonts
owner releases registered pool blocks after every slot is returned. Authored
checks cover pool growth, Lua/XML inheritance, callback ordering/state, recursive
and disabled clicks, injected pointer/cancel events, texture replacement/fallback,
slot alias rejection, owner/collection rejection and retained pixels after delete.
Original-data mode constructs unchanged `GlueCheckButtonTemplate` from
`GlueTemplates.xml`, using existing original fonts/styles and an authored label.
It checks InterfaceKey signatures before/after this subset and compares repeated
normal/checked/disabled rendering. This is not the complete AccountLogin UI or
its full post-creation digest gate. No physical-input acceptance is inferred.

Original-data checks exposed a retained texture geometry bug: an ordinary
non-tiled texture could be batched before its image-dependent rectangle resolved,
then keep the old vertices until a visibility change. Native texture layout now
invalidates its owning draw layer for all textures, matching retained vertex
ownership. A separate authored region-only translation/restore check covers this
without image I/O or parent mutation, and the original unchecked border must be
present before checked/disabled/re-enabled exact-pixel comparisons. Windows keeps
its original tiling-only invalidation path. Prior failing pixels remain evidence.

Compilation and qualification are pending GitHub Actions and downloaded-package
runtime checks. M3 remains open: EditBox, remaining widget/backdrop/model consumers,
complete signed ordered GlueXML execution, transport/authentication and observed
world entry are still needed. Clean data stays read-only and sign-in interactive.

The next login package must select `CSimpleEditBox` with aligned pooled strings/
textures, native-owned `CSimpleFontedFrameFont` storage and string-before-font
teardown. Existing `Os/Portable/Window.cpp` already supplies key state, clipboard,
text input and retained composition/candidate getters. OS language selection and
secure-input enable remain explicitly unavailable there; password masking alone
cannot qualify an unavailable OS secure-input capability. Reconcile the existing
IME enable count with focused edit boxes and restore focus/callback state on
unwind. Original candidate UI additionally owns `CSimpleMessageFrame`; it is not
yet selected. Keep unsupported consumers visible until their lifetimes are ported.

Read-only source inspection of the supplied AccountLogin XML identifies account,
password and token EditBoxes using `GlueEditBoxFont`, text insets, focus/tab/enter
handlers and tiled tooltip backdrops. Those required backdrops must also render
before claiming the original fields. Its remember-account CheckButton uses the
same original five images qualified here, with separate saved-account and sound
callbacks still owned by the future Glue application. This subset does not supply
those application callbacks. Complete ordered GlueXML creation and its second
digest comparison remain gates before interactive sign-in or transport testing.
