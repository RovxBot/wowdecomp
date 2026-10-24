# Original EditBox typing, focus and candidates

The native single-line login field selects original `CSimpleEditBox` editing,
FontString/cursor/selection regions, private font, Lua methods and XML factory.
Native capacity is 4095 UTF-8 bytes, 128 history entries and 32 candidates per page.
Multiline and native input-language selection remain visibly unavailable. Windows
bodies retain their original branches; the EditBox factory is extracted into the
shared factory file. No donor, game data or server policy changes.

All mutation belongs to the Frame owner. Close candidates and FontString text
consumers before the private font; close widgets before Scripts, image Services,
renderer/Fonts/Gxu/layout/XML/VM/device/context/queue. Pooled objects use aligned
native storage. Constructor failure releases text/private font ownership before
the base closes attached regions. Deletion copies overlap safely, aliased setters
retain their input, and explicit cursor/selection positions respect UTF-8 byte
boundaries. Original quoted-code, byte-index, numeric, history, clipboard, taint
and callback behavior remain selected. Password clipboard copies are empty.

Focus transitions guard reentry and object lifetime across callbacks. Each focused
field holds one SDL text-input request, including numeric/password fields; mode
changes do not imbalance it. This does not enable the unavailable OS secure-input
service. Transfer, hide, window focus loss and destruction retire the request.
Original root modifier state supplies shortcut/repeat behavior.

IME consumers copy the dequeued snapshot before the next Os dequeue. Preedit
replacement, committed text and cancellation update the original text state;
password fields accept committed text without displaying preedit/candidates.
Candidate Frame construction, row preparation and visibility changes occur before
root update traversal, preserving structural guards during input/resize callbacks.
Candidate messages belong to their EditBox and close before its private font.
Authored SDL injections are separate from physical keyboard/IME/language acceptance.

`NativeFrameHierarchyCheck` exercises typing, Unicode cursor/deletion, aliased
editing, selection, limits, clipboard shortcuts, password/numeric fields, history,
Lua/XML round trips, focus/reentry/teardown and retained candidate rows through the
actual SDL/Os/original Frame queues. Existing hierarchy and original resource
regressions run afterward. All client/dependency compilation is hosted in Actions;
local work is source inspection and Python source/launcher tests only.

Qualification is pending. Original-data runs use the read-only
`/mnt/games/3.3.5aClean/Data` with settings/cache outside the installation, original
archive authentication and existing subset signature checks. This authored field
is not complete AccountLogin or complete signed ordered GlueXML/full second digest
verification. Remaining widgets, login rendering, portable transport/authentication
and observed gameplay still gate M3. Sign-in stays interactive; preserve historical
qualification and the live server's tag/embedded-revision discrepancy.
