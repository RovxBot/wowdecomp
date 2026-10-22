# Original Frame language rules and Lua text setters

This work package selects original 12340 `LanguageProcess` rules 1, 2 and 4 for
native FontStrings and enables original Lua `SetText`/`SetFormattedText`.
The rules and static buffers move from `CSimpleRender.cpp` to `Language.cpp`;
Windows-selected language and setter bodies remain unchanged. The native target
selects the original Storm Unicode conversion source used by these rules. Native execution
uses the qualified Frame/Gxu/layout/VM owner and the existing guarded Lua method
boundary. Text/format numeric arguments retain original conversions rather than
the coordinate-method finite-float restriction. Every region and external script reference closes before Scripts,
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

## Qualification

[PR #68](https://github.com/RovxBot/wowdecomp/pull/68) is qualified at
`f0f522c467177cb1c044758c0f686d3001e9f3a4`, merged as
`2a0e3f5105fcb81064d287ba515fec2639c20151`. [Durable validation](language-validation.json)
records thirteen passing implementation PR workflows, 38 CTest checks per Linux
release/strict ASan/UBSan/leak configuration and full signature-enabled Windows
x64 compilation at matching-tree hosted checkout `fb6ba2fe87c3129ca078a8182c8ff952777c9ebf`.
Three archive digests and all 51 packaged executable hashes/types/notices verified.
Implementation, hosted checkout and merge share the same source tree.

Downloaded RX 7900 XTX / Mesa 26.2.5 / SDL X11 release and strict sanitizer packages
passed 30 authored/original-data input/UI/resource executions, three owner cycles
each, with empty stderr and zero workers. All fifteen release/sanitizer capture
pairs match; final original-data hierarchy captures retain the prior identity.
Intermediate readbacks assert authored text after actual Lua/language processing;
the final capture is authored hierarchy text through original fonts/UI shaders,
not original login. Four resource fixtures passed three cycles per mode.
Original-key archive authentication, selective member MD5 and before/after original
GlueXML signatures around eighteen GlueFonts definitions stayed enabled.

The missing native Storm Unicode provider produced a hosted link failure; a prior
region fixture then retained its superseded all-language-rejection expectation
(37/38 checks passed). Both failed runs remain in the record. Selecting the
unchanged provider and replacing that expectation with processed-text/cache
assertions plus unsupported-declension rejection resolved them. No detector was
disabled. Source review also added rejection of French article prefixes with no
following noun before the unchanged original rule can advance past its terminator.
Local source/launcher tests: 57 passed, one unavailable-CMake skip. Whitespace and
original language/renderer/setter token comparisons passed; no local compilation.

## Next original callers

Connect the original FrameXML registry/global `CreateFrame`, retained XML templates
and named parents, then remaining widget factories and nonempty Lua input handlers.
DB-backed declensions and broader locale/style qualification remain open. Complete
signed ordered GlueXML creation and its full post-creation digest gate, physical
IME/edit-box/candidate acceptance, full login rendering, portable transport and
observed gameplay remain open. Keep signatures and server revision discrepancies
intact; the user will sign in interactively. No new physical-input, Windows runtime,
macOS graphics or server acceptance is recorded. M3 remains incomplete.


## Subsequent base Frame XML integration

[PR #70](https://github.com/RovxBot/wowdecomp/pull/70) qualifies base Frame registry,
Lua CreateFrame, retained XML templates/named parents and Lua FontString template
creation; see [FRAME_XML.md](FRAME_XML.md). Earlier package identities and limits
above remain historical. Remaining widgets, broader XML/templates, nonempty Lua
handlers, physical IME/edit-box acceptance, complete signed ordered GlueXML/full
post-creation digest gate, original login and transport/gameplay remain open.
