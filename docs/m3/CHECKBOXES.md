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

Compilation and qualification are pending GitHub Actions and downloaded-package
runtime checks. M3 remains open: EditBox, remaining widget/backdrop/model consumers,
complete signed ordered GlueXML execution, transport/authentication and observed
world entry are still needed. Clean data stays read-only and sign-in interactive.
