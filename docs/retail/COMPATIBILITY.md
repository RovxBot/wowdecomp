# Later-client interface compatibility

The Alpha 0.2.0 workstream runs genuine Blizzard interface implementations on the
existing 12340 engine. Current acceptance and evidence: [#211](https://github.com/RovxBot/wowdecomp/issues/211).
The first native prerequisite landed in [#212](https://github.com/RovxBot/wowdecomp/issues/212),
[PR #213](https://github.com/RovxBot/wowdecomp/pull/213). Complete Edit Mode controls,
action-bar movement and layout persistence are still acceptance requirements.
Lua/bootstrap implementation and remaining acceptance are tracked in
[#215](https://github.com/RovxBot/wowdecomp/issues/215),
[PR #216](https://github.com/RovxBot/wowdecomp/pull/216).
The bounded secure-delegate prerequisite is
[#217](https://github.com/RovxBot/wowdecomp/issues/217),
[PR #218](https://github.com/RovxBot/wowdecomp/pull/218).

## Reference boundary and versions

[Machine-readable pins](../../Tools/LinuxClient/RetailUI/references.json) are the
source of truth for reproducible inspection. No exported Blizzard UI, textures,
fonts, models or other game assets are included in this repository or its builds.

| Reference | Selected revision / version | Role and import decision |
| --- | --- | --- |
| [Gethe Forever](https://github.com/Gethe/wow-ui-source/tree/9dc84f0d3bbae1768ac3405b3716000bd93eda06) | `9dc84f0d3bbae1768ac3405b3716000bd93eda06`, 1.60.1 (70862) | Primary interface implementation; Camelot game / Mainline family. Reference only. |
| [Gethe retail](https://github.com/Gethe/wow-ui-source/tree/297d7aaaa736f346b3e036f38ee088eaf7f8c07f) | `297d7aaaa736f346b3e036f38ee088eaf7f8c07f`, 12.1.5 (70783) | Later retail comparison; not interchangeable with Forever. Reference only. |
| [reckhou exports](https://github.com/reckhou/WoWBlzCode/tree/7245053a9f2cab5ea5e1ac5221489329d28b320c) | `7245053a9f2cab5ea5e1ac5221489329d28b320c`; README identifies Forever 1.60.1 (70658), retail 12.1.5 (70575) | Older, separately identified API documentation and comparison. Do not label it 70862. |
| [WarcraftXL client](https://github.com/WarcraftXL-Labs/client/tree/fdb9ad1819e68085d4cde4e33b2a17f772fcf85d) | `fdb9ad1819e68085d4cde4e33b2a17f772fcf85d`; 12340, README SDK ABI 0.1.46 | Source-level native seams / binding table. No import. |
| [WarcraftXL core](https://github.com/WarcraftXL/core/tree/f39aa1c9f2020d0968def3de895283cf0412e7cd) | `v1.1`, `f39aa1c9f2020d0968def3de895283cf0412e7cd`; requested `wxl-core` URL redirects here | Shared modern-format contracts, GPL-3.0-or-later headers. No import. |
| [WarcraftXL experimental](https://github.com/WarcraftXL/core/tree/2ae5bbb1c8b4c4a160352bcd7d990dc90d1fa93f) | `2ae5bbb1c8b4c4a160352bcd7d990dc90d1fa93f` | Actual older M2/ADT/WMO translators and Windows detours; WraithEngine GPL-3.0-or-later headers. Reference only. |
| [Source-client ABI core](https://github.com/WarcraftXL-Labs/core/tree/e393e1be00e57b30ea919b047ae292d41f59da0a) | `e393e1be00e57b30ea919b047ae292d41f59da0a` | Distinct source-client/runtime integration; do not conflate forks. No import. |

The UI mirrors provide Lua/XML and generated API descriptions, **not retail
client binaries or proprietary native engine source**. Neither primary UI mirror
supplies a repository licence granting redistribution of the exported interface.
The reckhou README expressly identifies a Blizzard-owned export. The WarcraftXL
client's [NOTICE](https://github.com/WarcraftXL-Labs/client/blob/fdb9ad1819e68085d4cde4e33b2a17f772fcf85d/NOTICE.md)
expressly grants no rights over its decompiled tree. Third-party components keep
their own notices. No WarcraftXL or Blizzard implementation was copied for this
prerequisite; its native helper and authored fixtures are product-authored.
Owning a game does not establish redistribution permission.

| Layer | Responsibility / boundary |
| --- | --- |
| Blizzard Lua/XML | Actual control construction, system mixins, selection, snapping, dialogs, account-setting logic and preset definitions. Supply permitted, pinned files externally. |
| Native Frame/FrameXML/FrameScript | Object and callback ownership, XML interpretation, frame geometry, input, secure-state admission, Lua APIs and persistence. Implement missing behavior here without pretending services exist. |
| Assets | Atlases, locale strings, fonts and textures are separate dependencies. A Lua export is not an asset pack or FileDataID resolver. Preserve original signed data. |
| Rendering/animation | Gx and Model2 must actually support each resource/material. Route new GPU work to #133/#134; no OpenGL development. |
| Server/data | Original 12340 actions, cooldowns, bags and unit data remain authoritative. New account-layout synchronization, specializations, auras or customisation schemas cannot be inferred from UI call sites. |

## Edit Mode dependency graph

The pinned [TOC](https://github.com/Gethe/wow-ui-source/blob/9dc84f0d3bbae1768ac3405b3716000bd93eda06/Interface/AddOns/Blizzard_EditMode/Blizzard_EditMode.toc)
selects Shared files, Mainline preset/override files and Camelot constants/utilities/
system overrides. Camelot is not the original Wrath UI. Its
[util](https://github.com/Gethe/wow-ui-source/blob/9dc84f0d3bbae1768ac3405b3716000bd93eda06/Interface/AddOns/Blizzard_EditMode/Camelot/EditModeUtil.lua)
uses MainActionBar, MicroMenu, BagsBar and newer tracking containers, with a
different bottom-bar hierarchy from 12340's MainMenuBar.

The [audit tool](../../Tools/LinuxClient/RetailUI/audit_reference.py) traverses all
required addon TOCs and XML Include/Script file edges for Camelot/Mainline in the
game environment. It fingerprints 381 files / 386 edges in the selected export;
there are no missing files or cycles in that declared graph. The fingerprint
also covers 69 referenced template-provider files, including implicit providers. It records optional
addon edges separately, all inherited template providers, mixin names, resource
references, and lexical native-namespace/method call sites. It neither executes
Lua nor proves runtime closure: dynamic globals, bootstrap order, secure loading,
asset lookup and conditional gameplay registrations remain unresolved inputs.
The whole graph is emitted as metadata outside Git, never as copied UI code.

Direct dependencies are UIPanelTemplates, HelpPlate, ManagedFrameSystem,
GameMenuEsc and UIParentUtil. Their transitive declared dependencies also include
SharedXMLGame, SharedXML, SharedXMLBase, ProjectConstants, Fonts_Shared,
ScriptErrors, PrintHandler, Menu, Colors, Narration and LoadLocale. The provider
index additionally identifies these undeclared bootstrap edges:

| Template | Provider outside the declared closure |
| --- | --- |
| `NewDefinitionsCheckerTemplate` | Settings_Shared/NewDefinitionsFramework.xml |
| `SecureFrameTemplate` | FrameXML/SecureTemplatesBase.xml |
| `UserScaledFrameTemplate` | AccessibilityTemplates/UserScaledElementTemplates.xml |
| `CooldownFrameTemplate` | FrameXMLUtil/Mainline/Cooldown.xml |
| `ActionBarFlyoutButton-IconFrame` | ActionBar/Shared/ActionButtonComponentTemplate.xml |

These are dependencies to qualify or bound, not instructions to import the whole
framework. SharedXML supplies layout mixins, pools, callback/event registries,
AnchorUtil, NineSlice, secure utilities and menu controls. ManagedFrameSystem
reparents and lays out visible managed frames and listens to EventRegistry.
FrameXML inheritance alone does not supply any of these Lua implementations.

| Edit Mode contract | 12340 support / remaining work |
| --- | --- |
| [Composition/templates](https://github.com/Gethe/wow-ui-source/blob/9dc84f0d3bbae1768ac3405b3716000bd93eda06/Interface/AddOns/Blizzard_EditMode/Shared/EditModeTemplates.xml) | New native opt-in frame `mixin`, script `method`, `inherit=append/prepend` prerequisite below. `Line`, DropdownButton, intrinsic definitions and modern loading tags are not implemented by it. |
| [System registration/anchors/scaling](https://github.com/Gethe/wow-ui-source/blob/9dc84f0d3bbae1768ac3405b3716000bd93eda06/Interface/AddOns/Blizzard_EditMode/Shared/EditModeSystemTemplates.lua) | Original SetPoint/GetPoint/scale/movement exist. Actual mixins override them, maintain snapped-frame graphs and compensate scale offsets. Ordinary native `Mixin`/`CreateFromMixins` are available below. Reviewed callbacks, managed layouts, Selection children and real-frame adapters remain required. |
| [Layout manager/persistence](https://github.com/Gethe/wow-ui-source/blob/9dc84f0d3bbae1768ac3405b3716000bd93eda06/Interface/AddOns/Blizzard_EditMode/Shared/EditModeManager.lua) | No `C_EditMode` namespace, layout enumeration, account-setting store, active-layout selection or synchronous `EDIT_MODE_LAYOUTS_UPDATED` event. No successful save is simulated. Define a local, versioned external-state store before binding these APIs; retail server storage is unavailable. |
| [Layout schema/API](https://github.com/reckhou/WoWBlzCode/blob/7245053a9f2cab5ea5e1ac5221489329d28b320c/Forever/BlizzardInterfaceCode/Interface/AddOns/Blizzard_APIDocumentationGenerated/EditModeManagerDocumentation.lua) | Layouts carry active index, layout name/type/interfaceStyle, systems, settings and named anchor targets. The secondary export documents 12 `C_EditMode` APIs. Primary source additionally calls `ConvertLayoutInfoToHyperlink`; its absence from that older index is a version gap, not evidence of support. |
| Cancel/reset/Classic | Manager snapshots/dirty settings, RevertAllChanges, per-system default anchors and preset tables must operate on the existing frame. A Forever Classic preset is not automatically the untouched 12340 layout; retain a separately captured original-layout restore point. |
| Selection/resources | Actual dialogs require atlases, NineSlice, modern dropdown/slider callbacks and locale tokens. The root EditMode XML references 12 resource names. Native CreateLine/SetColorTexture/SetShown/SetResizeBounds and newer widget facilities remain separate gaps. |
| Secure actions | Keep original ProtectedFunctionsAllowed / CSimpleTop admission and combat restrictions. Modern secure delegates, secret-value restrictions and cross-owner handler inheritance are not qualified. XML composition grants no ability to move a protected action bar in combat. |
| Action buttons | Original action buttons, keybindings, cooldowns, tooltips, page/state drivers and possession/vehicle transitions must remain attached to the real bar. Camelot's actionButtons/container and MainActionBar structures need an explicit adapter; do not replace them with copied buttons. |
| Newer services | Calls include `C_AddOns`, `C_GameRules`, `C_PlayerInfo`, `C_UnitAuras`, specialization events and newer frame systems. Only backed 12340 behavior may be exposed; unavailable newer server data stays unsupported. |

## Native XML compatibility v1

The full Linux source-preparation path copies `RetailUI/XMLCompat.*` into the
actual Frame library and adapts CSimpleFrame::LoadXML/LoadXML_Scripts. It is
selected only with:

```sh
export WOWDECOMP_RETAIL_UI_COMPAT=forever-1.60.1-70862-v1
```

No source directory is mounted by that variable. It enables only the following
parser operations for separately supplied XML. Original data and its signatures,
Glue ordering/digest, addon taint, login and server policy are unchanged. Original
XML takes its existing path whenever compatibility is disabled, including addon
attributes the original engine ignored. Within the enabled compatibility path,
missing/unsupported inputs report errors; direct helper calls also reject a
missing or incorrect version.

- Frame `mixin="FirstMixin, SecondMixin"`: resolve existing global tables in
  order, stage their string fields, then copy into the existing script object.
  Later fields override earlier fields. Preflight prevents partial publication
  on missing tables, invalid names or non-string keys; native slot zero is never
  copied. Limit: 32 tables, 255-byte identifiers, 4096-byte list. Dotted names and
  non-frame region mixins are unsupported in v1.
- Script `method="Handler"`: bind the instance's existing function during XML
  loading, using the original ScriptIx and event-admission path. Missing methods
  and ambiguous method/function/body declarations fail visibly.
- `inherit="append"` / `"prepend"`: compose an existing handler with a new
  method, global function or original-wrapper inline body. Preserve self, event,
  all arguments including trailing nil, call order, and normal Lua errors; an
  error stops the chain. Registry references are retired on replacement/deletion.
  Cross-owner script chains are explicitly refused. No VM taint is cleared.

This prerequisite does **not** publish `C_EditMode`, fake saved layouts, a new
Edit Mode addon or a visually similar control panel. Windows/macOS do not select
the helper yet. Their compatibility implementation and hardware acceptance need
separate qualification. Windows regression compilation still checks the original
product graph.

## Native Lua composition and bootstrap boundary

The same version opt-in registers `Mixin(object, ...)` and
`CreateFromMixins(...)` during the actual full Linux FrameScript VM initialization,
plus `table.contains` required by genuine TableUtil/EnumUtil.
Disabled or differently versioned VMs retain their original globals. The
implementation lives in FrameScript; XML composition continues in Frame.

The selected [native API declarations](https://github.com/Gethe/wow-ui-source/blob/9dc84f0d3bbae1768ac3405b3716000bd93eda06/Interface/AddOns/Blizzard_APIDocumentationGenerated/FrameScriptDocumentation.lua)
declare both APIs and disallow secure hooks. The selected
[Mixin.lua](https://github.com/Gethe/wow-ui-source/blob/9dc84f0d3bbae1768ac3405b3716000bd93eda06/Interface/AddOns/Blizzard_SharedXMLBase/Mixin.lua)
captures the native functions for CreateAndInitFromMixin/CreateSecureMixinCopy;
its SecureMixin loop supplies the source-level ordinary-table comparison.
The mirror contains no native implementation or retail binary to establish
undocumented native edge-case parity. No source implementation is imported.

- Copy present table fields shallowly, in argument order; later fields win.
  Functions, nested tables and arbitrary Lua keys retain their identities.
  No source metatable or inherited `__index` fields are copied. Existing target
  method lookup remains available; normal assignment honors `__newindex`.
- Return the same target, or a fresh ordinary table for CreateFromMixins.
  Sources must be tables, including every variadic argument. Bad arguments and
  assignment errors propagate through the normal VM. Earlier assignments can
  remain when a later source fails; this is distinct from XML's preflighted list.
- Preserve the engine object's lightuserdata slot zero. Refuse copying a native
  object as a mixin or replacing its identity. Ordinary Lua-table numeric keys,
  including zero, are supported. These refusals bound our native ownership
  contract rather than asserting undocumented retail behavior.
- Retain normal VM taint, function ownership and GC barriers. Keep no native
  registry roots. The existing hooksecurefunc rejects either native API, also
  through aliases, and otherwise retains its existing behavior. Composition
  grants no protected-frame/combat admission. Modern secret values, secure
  cross-taint delegates and forbidden-frame semantics remain unqualified.

`table.contains` follows the selected LuaTableExtensionsDocumentation contract:
search all present values, including associative/sparse entries, using Lua
equality and its ordinary `__eq` errors. Return one boolean; accept a nil search
value and reject a non-table source. It does not traverse inherited fields.

Independent external-source qualification loads genuine Mixin, TableUtil,
EnumUtil, FunctionUtil, MathUtil and AnchorUtil in their relative TOC order.
It exercises actual construction/initialization, captured native functions,
SecureMixin's ordinary-table result, enum inversion/membership, anchor values and
grid-layout parameters. EnumUtil.MakeEnum is a Lua helper; it does **not** supply
the native `Enum` namespace. No callback/pool or rendered-frame result follows
from this subset.

The ordered diagnostic follows required addon dependencies before TOC entries,
preserves selected Camelot/Mainline file order, and stops at the first failure.
XML files remain construction barriers; the probe never skips their controls,
resources, Include/Script order or implicit template providers to run later Lua.
The full declared sequence reaches Fonts_Shared/Mainline/Fonts.xml after genuine
ProjectConstants, Camelot/ProjectConstants and LoadLocale. It then requires an
initialized client/resource loader, including selected modern resource support. This
probe limitation is separate from a demonstrated integrated-engine runtime stop.
The ordered SharedXMLBase prerequisite begins with ScriptErrors, whose actual
CreateFromMixins/Init previously reached missing native CreateSecureDelegate.
The bounded implementation below advances that trusted-entry path; its current
continuation and remaining security limits are tracked in #217.
CreateCounter's returned closure requires securecallfunction; loading MathUtil
does not qualify its secure execution. CallbackRegistry and Pools additionally
require backed secure calls, forbidden
frames and secure containers. Independent file diagnostics must not be reported
as successful full Edit Mode bootstrap.
The focused native security follow-up is [#217](https://github.com/RovxBot/wowdecomp/issues/217).

## Selected enum and native font resources

The versioned full Linux FrameScript registration supplies only the reached
70862 `Enum.LFGRole` (Tank=0, Healer=1, Damage=2), its documented metadata
(NumValues=3, MinValue=0, MaxValue=2), and
`Constants.LFG_ROLEConstants` (LFG_ROLE_NO_ROLE=-1, LFG_ROLE_ANY=3).
The exact [LFGConstantsDocumentation](https://github.com/Gethe/wow-ui-source/blob/9dc84f0d3bbae1768ac3405b3716000bd93eda06/Interface/AddOns/Blizzard_APIDocumentationGenerated/LFGConstantsDocumentation.lua)
and font XML hashes are in the reference lock. Registration owns ordinary Lua
tables through each VM's registry; it rejects foreign roots/tables, metatables,
changed membership, numeric strings and conflicting values on re-registration.
Normal absent-key lookup still returns nil. No metatable invents values. VM close
retires the ownership roots. Disabled/wrong-version VMs keep original globals.

These role numbers select presentation names in genuine TextureUtil. Original
12340 GetLFGRoles/SetLFGRoles use leader/tank/healer/damage bits 1/2/4/8;
those APIs, packets and server functionality are unchanged. No modern LFG service
is inferred from the presence of constants.

The production `FrameXML_ProcessFile` now admits a bounded `FontFamily` subset
under the same opt-in. It selects exactly one `Member alphabet` from the existing
SFile locale: roman for enUS/enGB/frFR/deDE/esES/esMX, korean for koKR,
simplifiedchinese for zhCN, traditionalchinese for zhTW, russian for ruRU.
Missing/unknown/duplicate alphabets and unsupported locales fail without an
inferred fallback. Only the selected member's font file is required. The pinned FontDocumentation declares a family result as `SimpleFont`; UI.xsd
defines Member/Font structure, and SimpleFontAPI separately declares
GetFontObjectForAlphabet. This subset constructs only the selected alphabet;
that newer accessor and multialphabet behavior remain unsupported. A family
registers an actual named `CSimpleFont` through the original font factory,
including when marked virtual, consistent with original named font resources.

Selected Font supports file plus numeric `height`, inherited named fonts,
outline NONE/NORMAL/THICK, monochrome, spacing, Color, and Shadow with an
explicit Color and either x/y attributes or an Offset child. Inheritance-only
height/outline/monochrome overrides are rejected; these need an explicit font file. The helper validates the schema, inheritance,
finite scalar bounds and global ownership before using the existing XML font
loader, Services/TextBlock, Gxu and FreeType file construction. Height and shadow
syntax are explicitly translated to the original scalar nodes; other tags and
flags are not aliases. Staging validates required files before publication or
replacement. Existing font identities and original inheritance/consumer updates
remain authoritative; destroying the font registry invalidates Lua slot zero and
removes globals before its metatable and VM close. Optional shadow storage is
initialized before comparisons. Native method dispatch checks through the true
FrameScript_Object base, since fonts are not CScriptObject subclasses; the
existing opaque dispatcher signature is preserved.

Unsupported selected attributes, notably `slug`, fail explicitly. The pinned
Fonts_Shared/Mainline/Fonts.xml requests slug rendering after its initial six
families. A new rendering mode is outside this resource subset; route required
GPU work to #133/#134. Multialphabet shaping/fallback, new glyph/rendering formats,
FileDataID/atlas resolution, intrinsic definitions and complete modern widget
construction remain unqualified. Original signed MPQ/Glue traversal and addon
loading keep their existing path; compatibility does not mount external sources.

`RovRetailXMLCheck --resources /absolute/authored-font.ttf` initializes the real
font metatable, CPU resource dimensions, SFile direct reads and FreeType, then
exercises the production FrameXML file traversal, font identities, inheritance,
replacement, invalid inputs and repeated resource/VM lifetimes. This headless
harness does not initialize a gameplay root, GPU, input, transport or server. Its
native font objects replace earlier table markers only for font resources; the
independent Camelot utility fixture still uses table-only frame markers.
`--resource-sequence ASSET_ROOT LOCALE FILE...` follows supplied Lua/XML files
in order and stops on the first failure. The harness alone sets direct file
access/current directory; the application retains its original data policy.

For unchanged genuine inputs, use the hash-verifying audit wrapper:

```sh
python3 -B Tools/LinuxClient/RetailUI/audit_reference.py "$SOURCE" \
  --binary "$PROBE" --binary-sha256 "$PROBE_SHA256" --mode constants
# Also --mode texture, helpers, script-errors, base-bootstrap and bootstrap.
python3 -B Tools/LinuxClient/RetailUI/audit_reference.py "$SOURCE" \
  --binary "$PROBE" --binary-sha256 "$PROBE_SHA256" \
  --mode resource-bootstrap --assets "$EXTERNAL_FONT_ROOT" --locale enUS
```

Use `--mode resources` for the standalone genuine Fonts.xml sequence. Keep both
source and font roots external; no exported source/font enters Git or artifacts.
[#215](https://github.com/RovxBot/wowdecomp/issues/215) / [PR #219](https://github.com/RovxBot/wowdecomp/pull/219)
track qualification and actual ordered stops; selected slug resource admission
continues in [#220](https://github.com/RovxBot/wowdecomp/issues/220).

The first gameplay adapter remains **12340 MultiBarBottomLeft**, the existing
HorizontalMultiBar1 instance in original MultiActionBars.xml. Preserve its twelve
original action buttons, ActionButton_OnLoad, bindings, tooltips, cooldowns and
MultiActionBar_Update visibility. It avoids starting with the broader
MainMenuBar/vehicle/possession hierarchy. Before attaching the genuine
EditModeActionBarSystemTemplate, qualify its inherited EditModeSystemTemplate,
real Selection children/providers and atlases/fonts, then manager registration,
managed-frame anchoring/scaling and snapped-frame behavior. Supply backed local,
versioned layout storage and events before exposing C_EditMode save APIs. Capture
the original anchors as the Classic restore point, preserve protected-frame
admission, and qualify save/restart/restore/cancel/reset and combat restrictions
with the operator. No actual bar movement follows from font construction alone.

## Bounded secure delegate prerequisite

The same versioned Linux opt-in supplies `CreateSecureDelegate` for **trusted
entry only**. This is a smaller prerequisite than the modern API's complete
security boundary. It admits the pinned ScriptErrors construction and ordinary
string invocation; it does not qualify error handling from tainted addon code.
Current qualification, failures and bootstrap stops belong to
[#217](https://github.com/RovxBot/wowdecomp/issues/217).

The selected FrameScriptDocumentation describes protected invocation with secure
execution taint, direct arguments/results and default
`wrapUntrustedFunctions=true`. It declares restrictions and forbids secure hooks
on the factory, but does not enumerate all native restrictions or establish hook
behavior for generated closures. The selected ScriptErrors creates its delegate
from the local Lua function RemoveKStrings, then supplies debug-local text.
These exports contain no native implementation. This implementation is authored
against the declared contract; no Blizzard implementation is imported.

The audited original `Script_securecall` increments `lua_taintexpected`, calls
`lua_pcall` with the original error handler, swallows failures and retags results
with caller taint. It does not establish the modern delegate contract. The VM
tracks TValue and GC/closure taint, propagates it on reads and executes tainted
Lua closures with their owner. Both interpreter profiling modes use those paths.
`luaD_pcall` closes pending upvalues and restores VM frames on errors; it resets
the active-closure flag, rather than restoring the entire taint context.
`hooksecurefunc` owns its original/hook closures through traced upvalues.

Crucially, 12340's `CScriptRegion::ProtectedFunctionsAllowed` also uses
`lua_tainted` for protected-action authority. The VM has no independent modern
delegate security context, secret-value representation or automatic untrusted
function-argument wrapper. Clearing addon taint to obtain secure execution could
therefore admit protected actions. Ordinary `pcall` error protection does not
solve that missing authority boundary.

The bounded implementation consequently:

- Requires an untainted creation and invocation context, a real untainted Lua
  function, and untainted direct argument values. Rechecks the retained callee
  on every invocation, including ownership changed after construction. Tainted
  creators, callees, argument values and untrusted function arguments fail
  explicitly before invoking the callee. It never clears entry taint.
- Accepts omitted/nil/empty options or `wrapUntrustedFunctions=true`. Rejects
  false/nonboolean settings, unknown fields, tainted options/fields, metatables
  and extra arguments. Secret/security-context options are unsupported. No
  secret APIs, forbidden containers, callback services or timers are fabricated.
- Retains the original function in one Lua-owned C-closure upvalue; ordinary
  argument identities, trailing nils and all results pass directly. Trusted
  function arguments keep their identity. Retains normal VM taint propagation
  inside the call and on returned values; this is not arbitrary untainting.
- Uses the actual VM's protected call, closes open captures on errors and
  restores execution/closure/allocation taint and propagation/reentry flags on
  success or failure. Propagates the original error object after restoration;
  the global error handler is not implicitly invoked. Reentrant calls obey the
  same admission, and can fail when the outer call has become tainted.
- Rejects secure hooks on the factory through aliases. Generated delegate hooks
  are also refused as an explicit local qualification limit, not as an inferred
  modern API guarantee. Unrelated legacy hooks retain their original handling.
  Uses no native registry owners and leaves collection/VM teardown to the VM.

An already untainted caller gains no new protected-action or combat permission.
Existing checks remain in effect when the callee reads tainted data or calls
`forceinsecure`. Supporting tainted entry and untrusted function wrapping needs
a separately audited VM authority boundary; it must not be added by setting
`lua_tainted` to null. Disabled/wrong-version VMs register none of these helpers.
Windows/macOS do not select this Linux compatibility module.

The hosted production fixture covers malformed/default options, large argument
lists, nil/multiple returns and identity, nested calls, exact Lua error objects,
failed user error handlers, open-upvalue unwinding, retained-callee ownership,
tainted admission/refusal and returned taint, hooks, weak GC and teardown with a
live delegate. It runs both VM profiling modes alongside disabled legacy VMs.
These are component checks, not initialized frame/combat/gameplay observations.

`audit_reference.py --mode script-errors` verifies the existing pinned graph,
loads unchanged ScriptErrors and invokes its actual captured
RemoveKStringsDelegate through probe-only lexical inspection. It exercises
success, a genuine bad-input Lua error and recovery, without substituting
debug/log/timer services. Use `--mode base-bootstrap` separately for the actual
ordered SharedXMLBase continuation and `--mode bootstrap` for the full order.
The Fonts.xml resource boundary still requires an initialized client loader.

Generated selected-build enum values are described in
Blizzard_APIDocumentationGenerated/EditModeManagerConstantsDocumentation.lua;
this export has no global-string value for HUD_EDIT_MODE_ERROR_COPY_MAX_LAYOUTS.
Require permitted, versioned actual native enum/constants and locale inputs.
Neither EnumUtil nor the enUS locale selector substitutes those resources.

For a real bar, EditModeSystemTemplate must construct its actual Selection
children and call OnSystemLoad on the existing bar. That method preserves base
SetPoint/SetScale/ClearAllPoints methods, registers with EditModeManagerFrame,
loads setting display information and connects Selection. The managed-container
path also depends on EventRegistry and actual layout containers. A narrow
MultiBarBottomLeft adapter can retain the original twelve protected buttons,
bindings, page/state drivers and ownership; it must capture the original anchors
before any manager mutation. Reuse existing parentKey binding; qualify modern
global-valued KeyValue, resource and widget support. Actual manager construction
and backed local C_EditMode storage/events must precede move/save/operator
qualification. Do not reparent or
move a protected bar in combat, or equate a native script object with that bar.

## Reproduce source and hosted checks

All compilation runs in Actions. The four existing full Linux profiles retain
release and strict ASan/UBSan/vptr/leak coverage plus the signature-enabled
Windows baseline. `rov_retail_xml` executes the production helper with actual
full-client Lua and XML nodes in both VM profiling modes. Authored Lua tables
and an actual registered FrameScript_Object exercise ordering, arguments,
errors, opt-in, malformed inputs, native identity, method inheritance,
secure-hook admission, weak-reference GC, registry retirement and ownership/taint.
Disabled VMs are recreated before each enabled VM, in both profiling modes.
These checks do not establish actual frame creation, rendering, secure-action
parity, login or gameplay.

Supply a permitted export or a checkout of the exact reference outside both this
repository and game installations. For an existing external checkout:

```sh
git -C /external/wow-ui-source checkout 9dc84f0d3bbae1768ac3405b3716000bd93eda06
python3 -B Tools/LinuxClient/RetailUI/audit_reference.py /external/wow-ui-source \
  --report /external/edit-mode-dependencies.json
```

The audit refuses a different declared-graph fingerprint. Download the exact
`full-linux-evidence-*` artifact identified by the relevant #212/#215/#217 evidence
record, extract `RovRetailXMLCheck`
and its checksum externally, and verify the retained executable hash before use.
It needs the same Linux runtime libraries as the full application. To execute
the genuine Camelot utility through the native mixin/method binder:

```sh
python3 -B Tools/LinuxClient/RetailUI/audit_reference.py /external/wow-ui-source \
  --binary /external/evidence/RovRetailXMLCheck \
  --binary-sha256 <hash-from-the-retained-hosted-checksum>
```

This loads the unchanged Camelot EditModeUtil and checks its actual center-system
and bottom-bar ordering through the native binder with table-only frame markers.
It does not construct gameplay frames. `RovRetailXMLCheck --load <external.lua>`
separately reports the first unsupported dependency of an unmodified file and
exits 1 on a Lua error; it does not supply mock services to force a load.

Use `--mode helpers` with the same audit command and a matching probe from
#215 to qualify the genuine helper subset. Use `--mode base-bootstrap` for the
ordered ScriptErrors/SharedXMLBase prerequisite, or `--mode bootstrap` for the
whole selected declared dependency sequence. Both modes retain the first
failure; neither skips an XML/resource barrier. Reports include both ordered
plans and all existing template-provider identities, with implicit provider
admission still unresolved. The probe's `--sequence <files...>` mode can also
load an explicitly identified Lua-only subset in one VM. Always distinguish
that subset from full ordered bootstrap. Keep reports/logs outside the repository.

For later operator acceptance, use a hash-verified TEST-SERVER release with
unchanged clean data and external state; the operator signs in interactively.
Do not yet enable the whole Forever addon. Once the missing services/resources
and one real action-bar adapter are implemented, verify enter/exit, drag/snap,
scale, save/restart/restore, cancel, reset and original 12340 layout restoration,
then all original actions/bindings/tooltips/cooldowns and combat rejection.
Record actual observations in #211 separately from this component test. Strict
integrated startup #210/#172 remains independent and unqualified by this fixture.

## Subsequent feature order

1. Finish the Edit Mode slice: modern composition helpers/callback/layout
   dependencies, resource resolver, local layout API store, then one real bar
   adapter and operator tests. Add further registered frames individually.
2. Blizzard [one-bag UI](https://github.com/reckhou/WoWBlzCode/blob/7245053a9f2cab5ea5e1ac5221489329d28b320c/Retail/BlizzardInterfaceCode/Interface/AddOns/Blizzard_UIPanels_Game/Mainline/ContainerFrame.lua): reuse 12340 container slots and item actions behind
   reviewed container/item bindings, then the genuine bag implementation and
   templates. Do not invent account/warband/reagent storage unavailable in 12340.
3. Enhanced [unit frames](https://github.com/reckhou/WoWBlzCode/tree/7245053a9f2cab5ea5e1ac5221489329d28b320c/Retail/BlizzardInterfaceCode/Interface/AddOns/Blizzard_UnitFrame): reuse qualified layouts/atlases and original unit events;
   adapt genuine frame implementations without exposing unsupported aura data.
4. [Cooldown manager](https://github.com/reckhou/WoWBlzCode/tree/7245053a9f2cab5ea5e1ac5221489329d28b320c/Retail/BlizzardInterfaceCode/Interface/AddOns/Blizzard_CooldownViewer): its TOC requires EditMode, ChatFrameBase,
   VisualAlerts and ManagedFrameSystem and selects the standard game type. Audit
   the APIs/flavor boundary before enabling it in Forever. A narrow
   player cooldown display can use backed 12340 spells; modern cooldown-set,
   charge/override/spec and secret-data APIs require explicit capability limits.
5. [Character customisation UI](https://github.com/reckhou/WoWBlzCode/tree/7245053a9f2cab5ea5e1ac5221489329d28b320c/Retail/BlizzardInterfaceCode/Interface/AddOns/Blizzard_CharacterCustomize): its TOC depends on CustomizationUI.
   Follow the [modern model workstream](MODERN_ASSETS.md)
   and an explicit mapping to supported 12340 choices. New races/options or
   server-persisted appearance fields need separately approved extensions.

Order follows shared infrastructure and available original data; none of these
steps authorizes importing the entire modern framework or changing the protocol.
