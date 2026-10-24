# Original MessageFrame and private font ownership

This package adapts original 12340 `CSimpleMessageFrame` to the existing native
Frame pools, private `CSimpleFontedFrameFont`, FontString/Gxu layout and renderer.
It is a prerequisite for EditBox candidate messages and other original Glue widgets.
The private font implementation and MessageFrame factory move into shared source
files without changing their Windows-selected bodies. No donor or game data is added.

Native factories expose MessageFrame through Lua CreateFrame, XML/templates and
canonical metatables. Private fonts and reference-counted row nodes use matching
C++ allocation/deletion. A MessageFrame closes rows and their FontStrings/text
blocks before its private font. Close widgets before Scripts, image Services,
renderer, Fonts/Gxu/layout/XML/VM/device/context/queue. Pools retain aligned native
slots through the Fonts owner's teardown.

Font/resize callbacks mark rows dirty. Root update resolves layout and prepares row
ownership before entering traversal; allocation, attachment and row retirement
therefore occur outside resize/input/update/collection callbacks. Original insert
order, offsets, rounding epsilon, pending-message copies, visibility and fading
remain. Text is valid UTF-8 up to 4095 bytes; the initial native surface bounds
pending messages to 1024 and visible rows to 256. Invalid dimensions/modes or
foreign-thread/collection mutation fail visibly. Public fields are borrowed state.

The hierarchy fixture checks pooled growth/alignment, XML/template/Lua round trips,
canonical/stale tables, pending messages, row growth/shrink/regeneration, insertion
order, fading pixels, callback admission, owner/collection guards and full teardown
across three owner cycles. Candidate-like text is authored; original-data mode
uses the previously authenticated original Friz font/UI shaders with existing
GlueFonts subset signature checks. It does not deliver IME candidates to EditBoxes
or execute complete signed GlueXML or its full post-creation digest comparison.

All client/dependency compilation runs in Actions. Hosted release/strict sanitizer
and signature-enabled full Windows checks, downloaded package verification and
original-data GPU execution are required before qualification/merge. The local
source/launcher suite currently passes 57 checks with one unavailable-CMake skip.
Its first source-check attempt failed because the audit fixture still held the
preceding allowlist. The first hosted pressure fixture omitted ResizePending before
PrepareRows and failed to exercise growth; the corrected fixture follows the root's
layout order. Retain those failures separately from later qualification. Native
row growth constructs temporary owners before publishing counts and rolls them
back on allocation/admission failure.

EditBoxes, remaining widgets, complete signed ordered GlueXML/full second digest,
actual login, transport/authentication and observed gameplay remain open. Clean
data stays read-only; signatures/server revision discrepancies and interactive
user sign-in remain intact. M3 is incomplete.
