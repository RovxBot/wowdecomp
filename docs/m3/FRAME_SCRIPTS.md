# Bounded original region Lua methods

Frame `GetAlpha`, `GetEffectiveAlpha` and `SetAlpha` retain original 8-bit
clamping and inherited integer multiplication. Finite scalar alpha writes use
the existing pre-collection script-text guard: update callbacks may change color,
while collection, rendering, foreign threads and expired objects reject access.
Hierarchy/visibility/deletion guards remain separate. Native regions invalidate
copied mesh colors on every effective-alpha change; retained text also receives
the original color/shadow notification. Repeated fades and restoration are covered
by the hierarchy fixture. Integrated callback disposition is tracked in
[#110](https://github.com/RovxBot/wowdecomp/issues/110) and
[#95](https://github.com/RovxBot/wowdecomp/issues/95); full login rendering remains
[#96](https://github.com/RovxBot/wowdecomp/issues/96).

This selects original 12340 region, Frame, title and FontString metatables on the
native ABI. `Frame::Native::Scripts` belongs inside the Fonts owner and outside
all region/script objects. Close objects and external function references before
its metatables, then renderer/Fonts/Gxu/layout/XML/VM/device/context/file queue.
All client/dependency compilation runs in GitHub Actions.

The first surface exposes original identity/parent/name, dimensions/anchors,
Frame region/child enumeration, visibility and title/pooled FontString creation.
`CreateFontString` uses the qualified original pool, named font inheritance and
`$parent` naming. Canonical live tables are required for object arguments; raw
userdata copied into another table never admits a pointer. Native exceptions
unwind before conversion to Lua errors. Geometry arguments must fit finite floats;
point indices are bounded before integer conversion. Traversals reject selected
methods before allocation or mutation. Unsupported methods report a Lua error.
The original draw-layer converters move unchanged into shared FontXML helpers.
Original Windows-selected method bodies remain unchanged.

This deliberately leaves language processing and `SetText`/formatted text closed.
Authored rendering checks set C++ text with explicit `process=0`, then exercise
real Lua visibility, anchors and font objects through the original hierarchy and
renderer. This is not script language processing or original login composition.
Native XML virtual templates, widget factories, nonempty input handlers, texture/
animation/backdrop/scroll consumers and the Lua global `CreateFrame` remain open.

The existing three-cycle hierarchy fixture now checks named pooled Lua text,
canonical identities and parents, inherited fonts, geometry/anchors, title reuse,
invalid dimensions, missing templates, spoofed tables, cycles and callback guards,
render/repeat pixels, Lua visibility and stale tables after recursive deletion.
Other hierarchy/input/resource regressions retain their previous contracts.
Original-data execution uses read-only clean 12340/enUS data, original-key archive
authentication/selective MD5 policy and before/after GlueFonts-subset signatures.
Complete ordered signed GlueXML creation and its full post-creation digest gate,
physical IME/edit-box acceptance, actual login, transport/authentication and
observed gameplay remain open. M3 is incomplete; preserve server revision
discrepancies and interactive user sign-in.

## Qualification

[PR #66](https://github.com/RovxBot/wowdecomp/pull/66) is qualified at
`56aa592edfc7685c7234119aab874f55173bf61f`, merged as
`914a6d5b72a3edee185379559c5b56a2ebd9b273`. The [durable validation](frame-script-method-validation.json)
retains all thirteen passing PR workflows, 38 CTest checks per Linux release/strict
ASan/UBSan/leak configuration and full signature-enabled Windows x64 compilation
at matching-tree hosted checkout `3c9d0ba7edc0a4bc9004907a3cdafe1919e18a62`.
Implementation, hosted checkout and merge have the same source tree. Three artifact
archive digests, all 51 packaged executable hashes/types and notices verified.
Graphics remain Linux x86-64; prerequisite component matrices retain separate
actual ARM64 macOS and native Windows coverage.

Downloaded RX 7900 XTX / Mesa 26.2.5 / SDL X11 release and strict sanitizer packages
passed 30 authored/original-data input/UI/resource executions, three owner cycles
each, with empty stderr and zero workers. Four resource fixtures also passed three
cycles per mode. All fifteen release/sanitizer capture pairs match. Final original-
data hierarchy/input captures retain SHA-256
`55b0e2c731d888293851e66a21b2b7eccaa29405097fb4b6b1e65588f60c93cf`.
The inspected final capture is authored hierarchy text with original fonts/UI
shaders. Intermediate readbacks assert Lua-created FontString rendering, visibility
and exact repeats; they are not exported new captures or original login UI.

Each hierarchy/input cycle ran the new Lua factory/identity/layout/stale-table
fixture alongside qualified pool and prior hierarchy checks. Original-key archive
authentication and selective registered-member MD5 policy stayed enabled; original-
data cycles authenticated GlueXML before/after eighteen GlueFonts definitions.
Complete ordered creation and the full post-creation digest gate remain open.
Local source/launcher checks: 57 passed, one unavailable-CMake skip. Windows-selected
tokens in seven changed legacy Frame sources match immutable pre-PR base; both
extracted draw-layer converters match. Whitespace passed; no client/dependency
compiled locally. The initial graph-test allowlist fixture was updated for the
selected sources before hosted submission. No hosted implementation or actual
runtime failure occurred and no detector/assertion was disabled. A usage-limit
failure in automatic approval review prevented one sanitizer download/execution
attempt from running; after the user requested continuation, the resumed strict
checks passed. Superseded/duplicate runs remain separate historical records.

## Next original callers

1. Connect the original FrameXML factory registry and Lua global `CreateFrame`,
   named-parent XML processing and retained virtual templates/inheritance. Preserve
   original manifest order and borrowed XML nodes through creation.
2. Adapt original language processing before selecting Lua text/format setters;
   extend additional font/region/Frame methods in bounded packages. This package's
   text content uses explicit C++ `process=0`, not a replacement language service.
3. Install actual nonempty Lua input handlers and remaining widget factories only
   after their ownership, allocator/deletion and service contracts are adapted.
   Texture/image-mode, backdrop, scroll, animation/model and edit boxes remain open.
4. Execute complete signed ordered GlueXML with the full post-creation digest gate,
   then render actual login before interactive user sign-in. Physical mouse/IME/
   language/edit-box/candidate UI acceptance and transport/gameplay remain open.

No new physical-input, Windows runtime, macOS graphics or server acceptance is
recorded. M3 stays incomplete; preserve signatures, historical identities, server
revision discrepancies and interactive user sign-in.


## Subsequent language integration

[PR #68](https://github.com/RovxBot/wowdecomp/pull/68) qualifies the bounded original
language rules and Lua text setters in [LANGUAGE.md](LANGUAGE.md). The PR #66
observations above retain their historical C++ `process=0` text scope. Current
language/transformed-text evidence and remaining DB-backed declensions are separate;
XML registry/templates, other widgets, full login and gameplay remain open.


## Subsequent base Frame XML integration

[PR #70](https://github.com/RovxBot/wowdecomp/pull/70) qualifies base Frame registry,
Lua CreateFrame, retained XML templates/named parents and Lua FontString template
creation; see [FRAME_XML.md](FRAME_XML.md). Earlier package identities and limits
above remain historical. Remaining widgets, broader XML/templates, nonempty Lua
handlers, physical IME/edit-box acceptance, complete signed ordered GlueXML/full
post-creation digest gate, original login and transport/gameplay remain open.
