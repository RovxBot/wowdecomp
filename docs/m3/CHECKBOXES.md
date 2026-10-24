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

[PR #79](https://github.com/RovxBot/wowdecomp/pull/79) is qualified at
`2f0035fef2a64bbb6fe3c6fefbeb2ad9c099b198`; the [durable record](checkbox-validation.json)
retains exact workflow, checkout, package and runtime identities. Final-source
Linux GL release and strict ASan/UBSan/leak configurations each passed 38 CTest
checks. Full signatures-enabled Windows x64 compilation passed at the same source.
Initial production passed twelve other PR workflows, including actual ARM64
execution; the four changed GL production/fixture/contract entries are excluded
from the 3,890-entry unchanged-source proof. Their PR checkout was
`8f92de48d3165f9682800385e97b4dd536f1a50f`, with an identical implementation tree to
`a2fc8f325fe12cf8a7b9fcd6d42e725c01b73ec8`. Earlier runs are not relabeled final-head
checks. Seven archives and all 67 binary hashes/types/notices verified.

Downloaded RX 7900 XTX / Mesa 26.2.5 / SDL X11 packages passed sixteen authored/
original-data wrapper executions, three owner cycles each, with empty stderr and
zero workers. Eight final regression capture pairs, five original blue Button
pairs and five original CheckButton pairs match between release and sanitizer.
All eight preceding regression captures also match. The original checkbox PPM
SHA-256 is `5b61cb6819d9e045d0ca435261385030c5bc3b8811a657db736d87f731dc5f3d`;
visual inspection shows the original border/tick and legible yellow original font
with an authored label. Each original CheckButton became ready in two frames.
InterfaceKey before/after subset checks and original signaturefile authentication
(28,948 records) passed; selective registered-member MD5 policy stayed enabled.
Image/Gxu and all four resource fixtures also passed three cycles per mode.

The initial hosted compile failed on fixture focus-event names. Its correction
passed hosted checks but original-data pixels then exposed the stale texture
geometry; both failed attempts remain recorded. Release-only GDB pixel/layout
inspection identified a missing initial border, not a changed expected image.
The initial restricted executor could not access X11; final execution used the
normal desktop session without tracing, with every strict detector unchanged.
Local source/launcher checks: 57 passed and one unavailable-CMake skip; Windows
selected tokens, original factory extraction and whitespace checks passed. No
client or dependency compiled locally. M3 remains incomplete: EditBox, remaining
widgets, complete signed GlueXML/login, transport/authentication and observed
world entry remain open. Clean data stays read-only and sign-in interactive.

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
