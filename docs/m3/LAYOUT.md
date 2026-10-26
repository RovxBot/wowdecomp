# Original Frame layout and anchors

This package follows the qualified [authored image mip/readiness work](IMAGE_MIPS.md).
It selects actual 12340 `CLayoutFrame.cpp` and `CFramePoint.cpp`, scalar coordinate
and rectangle/vector consumers under a separate native ABI. Original anchor names,
`FramePointToString` and `LoadXML_Dimensions` are moved unchanged from `LoadXML.cpp`
to shared `LayoutXML.cpp`; the two scalar layout comparisons likewise move to
`Tempest/LayoutMath.cpp`, so Windows and native targets use the same bodies.
Native entry guards are adaptations; historical OFFSET/VERIFY tags describe the
Windows bodies, without a new binary verification claim. No donor code is imported.

`Frame::Native::Runtime` lives inside the XML and FrameScript owners. Its lease
prevents XML destruction while installed; the XML owner already pins FrameScript.
All layout mutation, queries and point-pool operations belong to that owner thread.
Finish resize callbacks, destroy frames and standalone points, then close layout,
XML and FrameScript, in that order. Destroying layout objects from resize callbacks
is rejected. Public legacy fields are borrowed owner-thread state, not a separate
thread-safe API. Root owners must supply their screen rectangle; this package does
not provide the actual screen root/factory or SDL size invalidation yet.

Native constructors initialize layout/point flags before reading them, use portable
RTTI names and clear the point pool after the last consumer. The Windows constructor
and allocator bodies remain selected for the original client. Unsigned native
anchor indices are checked before indexing, and nonfinite coordinates/scales,
negative dimensions and nonpositive scales are rejected. Zero dimensions retain
original anchor-derived size behavior; depth retains its original 0.2 minimum.
The original qualification admitted 1,024 live layout objects. PR #84's integrated
Glue creation reaches that bound while retaining hidden screens and regions, so
the bounded population is now 8,192. Original SimpleHTML then exceeded the old
64-frame anchor bound; its supplied credits chain more than 1,000 regions.
Anchor paths are now bounded at 2,048, separately from the unchanged 64-level
Frame parent graph. The existing depth fixture admits 2,047 dependent objects
after its root and rejects the next edge, while retaining cycle rejection. The existing
layout fixture fills the population, rejects the next object, retires/reuses a
slot and closes all owners in three cycles. The existing MessageFrame partial-row
rollback test retains equivalent pressure at the new bound. See the
[continuation record](integrated-model-render-validation.json) for its separate
hosted identities; the original qualification below remains historical. Proposed
edges are validated over the whole graph with memoization, rejecting cycles and
excessive depth before mutating anchors, including repeated references.

The original anchor solver, resize dependency registration, protection propagation,
deferred/pending ordering, scaling, clamping, flattening and XML relative lookup
remain in the actual consumers. Native origin destruction schedules dependents
through original resize lists. A failed previously valid rectangle now propagates
invalidation to its dependents, preventing stale transitive rectangles after the
pending queue is processed. Original absolute XML dimensions convert both axes
through aspect compensation and DDC width; relative dimensions retain raw values.
This is not a new pixel-only layout system.

[M3 layout Actions](../../.github/workflows/m3-layout.yml) compiles Linux release
and strict ASan/UBSan/leak, actual ARM64 macOS and Windows native configurations.
`NativeLayoutCheck` repeats three complete owner/pool cycles, exercising all nine
anchors, size inference, scale/depth/clamp, flatten/unflatten, protected anchors,
deferred/unloaded objects, dependency destruction, invalid inputs, cycle/depth
bounds, foreign-thread rejection, callback lifetime rejection and original XML
absolute/relative/setAllPoints behavior at 4:3 and 16:9. The exact CMake graph is
audited; original signatures stay enabled. The separate Windows baseline compiles
the full x64 client. All client/dependency compilation runs in Actions.

## Qualification

[PR #39](https://github.com/RovxBot/wowdecomp/pull/39) is qualified at
`4aa176506079a35e81c3bf9d7108e7bff1d57df2`, merged as
`4cf5e0e536dca1bb96f06d8a8500e7ddd293c754`. The [durable record](layout-validation.json)
retains all thirteen passing implementation PR workflows, 15 CTest checks and
three layout cycles per Linux release/strict ASan/UBSan/leak, actual ARM64 macOS
and native Windows configuration. Five archive digests and all 51 packaged binary
hashes/types were verified, including full Windows x64 compilation with signatures
enabled at matching-tree checkout `8e4de17077fd39972148f7b0aa1101a28e711dd6`.
The separate Linux GL workflow passed its existing release/sanitizer regressions;
there is no new desktop GPU/rendered-output acceptance in this package.

Downloaded exact-source Linux release and strict sanitizer binaries each passed
three authored layout cycles and three unchanged original-data XML/signature
cycles, with exit 0 and empty stderr. The XML regression authenticated original
GlueXML/FrameXML content and parsed AccountLogin; it did not instantiate original
Frame objects. Its shutdown reports zero workers. Original-key package RSA and
registered-MD5 policy remain enabled. Local Python: 45 passed, one platform skip.
No client/dependency compiled locally. The missing scalar-helper link and the
fixture's incorrect X/Y root initialization remain failed evidence; original
`CRect` stores Y/X. Neither correction changes the solver or disables a detector.

## Next original callers

1. Connect actual `CScriptObject` name/global-table ownership and original
   `CSimpleFont`/`CSimpleFontable`/`CSimpleFontStringAttributes` factories to the
   FrameScript and Gxu owners. Preserve borrowed script-object lifetime, inherited
   font attributes, registered fontable lists and font handles through text blocks.
2. Select actual `CScriptRegion`/`CSimpleRegion` and `CSimpleFontString` consumers
   (the latter is declared in `CSimpleRender.h`), then the real `CSimpleTop` root,
   deferred destruction, batching and SDL size/font invalidation. This initial
   layout owner rejects destruction during resize callbacks; adapt the root's
   deferred destruction before enabling those callers.
3. Adapt `CSimpleTexture::SetTexture`'s `0x17` disk/atlas/priority request, actual
   image modes and `TextureGetAtlasInfo` coordinates. Preserve explicit supported
   source lookup/default filtering and diagnose unsupported TGA/streaming policy.
4. Connect original FrameXML factories, retained virtual templates and ordered Lua
   execution. Preserve raw signed traversal order and the second content-digest
   comparison in `CGlueMgr::Resume` before FRAMES_LOADED. Execute/render actual
   original GlueXML/login before the user's interactive sign-in.

This authored layout fixture adds no original-data Frame layout/rendering,
physical-input, Windows runtime, macOS desktop or server acceptance. Transport,
authentication and observed gameplay remain open. M3 is incomplete; preserve
server revision discrepancies and interactive user sign-in.
