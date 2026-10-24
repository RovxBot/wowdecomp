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

[PR #81](https://github.com/RovxBot/wowdecomp/pull/81) is qualified at
`9a8ffc528320d0ca12fa8f1b4e7120022b4ab62c`. The [durable record](message-frame-validation.json)
retains both final-source Linux GL release/strict ASan/UBSan/leak configurations
passing 38 CTest checks each. Full signature-enabled Windows x64 compilation
passed at separate source `9e034144c95186a60f93e292e44900183454e8c1`; later
production changes are native-only and Windows-selected tokens remain unchanged.
Three archive digests/all 51 packaged binary hashes/types/notices verified. Six
additional executables per GL manifest were built/checked on Actions but are not
shipped in these packages; their local binary types/hashes are not claimed.

Downloaded RX 7900 XTX / Mesa 26.2.5 / SDL X11 release and strict sanitizer packages
passed twelve three-cycle authored/original-data executions, with empty stderr
and zero workers. All 27 capture pairs and 21 preceding backdrop-package capture
comparisons match. The original-font message capture was inspected: three white
authored rows, including the candidate-like index line. Original signatures and
existing original Button/CheckButton/backdrop regression pixels remain intact.

All compilation ran in Actions. Local source/launcher checks: 57 passed with one
unavailable-CMake skip; Windows-selected token and whitespace comparisons passed.
Its first source-check attempt failed because the audit fixture still held the
preceding allowlist. The first hosted pressure fixture omitted ResizePending before
PrepareRows and failed to exercise growth; the corrected fixture follows the root's
layout order. Original-data execution then exposed an authored row-height
expectation that equated font size with row height; original font spacing and
pixel rounding contribute to the observed stride now used by row-size fixtures. Retain those failures separately from later qualification. Native
row growth constructs temporary owners before publishing counts and rolls them
back on allocation/admission failure.

EditBoxes, remaining widgets, complete signed ordered GlueXML/full second digest,
actual login, transport/authentication and observed gameplay remain open. Clean
data stays read-only; signatures/server revision discrepancies and interactive
user sign-in remain intact. M3 is incomplete.

## Next EditBox callers

`CSimpleEditBox.cpp` still needs native source selection and pooled factory/Lua/XML
admission. Its constructor creates a private font, FontString, three selection
textures and cursor; initialize fields and unwind partial construction before the
base Frame retires attached regions. Close candidate messages and text consumers
before their private font. Native `raw_name()`/Storm-placement allocations must use
aligned pools or matching C++ allocation/deletion as appropriate.

`CreateCandidatesFrame` currently allocates and shows an unparented tooltip frame
from input handling. Retain candidate/composition snapshots at dequeue and defer
candidate construction, row changes and show/hide until outside root input/resize
traversal; do not weaken the existing structural guards. This package's
`PrepareRows` supplies the row phase but does not yet connect that caller.

Use retained root modifier state for repeat/key shortcuts and preserve original
text/selection/cursor/clipboard/taint callbacks. Audit overlapping DeleteSubstring
copies, UTF-8 byte-boundary cursor operations, aliased insertion buffers and text
bounds before admitting the original native editing surface. Focus callbacks need
reentry/lifetime guards and balanced native text-input requests. SDL committed text
must also reach password fields; OsInputEnableSecureMode currently reports the
unavailable OS secure-input service and does not enable SDL text input. Native
language selection and physical IME acceptance remain separate open qualifications.

After EditBox/remaining widget factories, execute complete manifest order with
original InterfaceKey verification and the full second digest after creation;
then connect portable transport/authentication before interactive user sign-in
and observed GrimGuzzler world entry. Packet/crypto fixtures alone supply no socket
or authenticated session. Keep the live server's tag/embedded-revision distinction.
