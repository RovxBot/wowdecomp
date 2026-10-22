# Original Frame language rules and Lua text setters

This work package selects original 12340 `LanguageProcess` rules 1, 2 and 4 for
native FontStrings and enables original Lua `SetText`/`SetFormattedText`.
The rules and static buffers move from `CSimpleRender.cpp` to `Language.cpp`;
Windows-selected language and setter bodies remain unchanged. Native execution
uses the qualified Frame/Gxu/layout/VM owner and the existing guarded Lua method
boundary. Every region and external script reference closes before Scripts,
renderer/Fonts/Gxu/layout/XML/VM/device/context/queue.

Rule 1 chooses Korean particles using the original Hangul, Latin-letter and digit
classification, including the rieul exception. Rule 2 retains the original French
article/vowel decisions. Rule 4 uses the original FrameScript plural selection,
including all three plural configurations. Rule 3 needs the game declined-word
DBs and player/pet name caches and remains visibly unavailable. This is bounded
language processing, not qualification of all locales or original login.

Native processing admits valid UTF-8, at most 4095 bytes per intermediate pass,
complete particle/plural alternatives and article words, signed-32-bit decimal
counters and at most 64 passes. Original fixed 8192-unit/byte scratch buffers
remain owner-thread borrowed storage: copy a result before the next call. The
input bound also covers French article expansion before another validation pass.
Malformed or unsupported input throws before FontString text mutation; C++ scopes
unwind before conversion to a Lua error. Embedded image markup remains unavailable.
Direct C++ `process=0` retains its existing independent text contract.

The hierarchy fixture checks Korean/French/plural examples, escaped pipes, color
markup, nested rules, Unicode, numeric/nil setters, original positional formatting,
all three plural configurations, input/expansion bounds, malformed UTF-8/grammar,
unsupported declensions/images, absent fonts, traversal admission, unchanged text
on rejection, rendered text/repeat pixels and pool/cache teardown over three owner
cycles. It uses authored text with original or authored fonts; it does not establish
physical input/IME/edit-box acceptance or actual login composition.

Compilation and runtime qualification are pending GitHub Actions and downloaded
exact-source package execution. All client/dependency compilation runs in Actions;
clean original data stays read-only and outside Git/ordinary hosted CI. Preserve
original-key archive authentication, selective member MD5 policy and the existing
before/after GlueFonts-subset signature checks. Complete ordered signed GlueXML
creation and the full post-creation digest comparison remain open, as do XML
registry/templates, other widgets, full login, transport and observed gameplay.
The user will sign in interactively when ready; server revision discrepancies
remain recorded separately. M3 is incomplete.
