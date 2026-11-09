# Later-client interface compatibility

The Alpha 0.2.0 workstream runs genuine Blizzard interface implementations on the
existing 12340 engine. Current acceptance and evidence: [#211](https://github.com/RovxBot/wowdecomp/issues/211).
The first native prerequisite is [#212](https://github.com/RovxBot/wowdecomp/issues/212),
[PR #213](https://github.com/RovxBot/wowdecomp/pull/213). Complete Edit Mode controls,
action-bar movement and layout persistence are still acceptance requirements.

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
| [System registration/anchors/scaling](https://github.com/Gethe/wow-ui-source/blob/9dc84f0d3bbae1768ac3405b3716000bd93eda06/Interface/AddOns/Blizzard_EditMode/Shared/EditModeSystemTemplates.lua) | Original SetPoint/GetPoint/scale/movement exist. Actual mixins override them, maintain snapped-frame graphs and compensate scale offsets. Need reviewed callbacks, `Mixin`/`CreateFromMixins`, managed layouts, Selection children and real-frame adapters. |
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
XML lacking modern attributes takes its existing script path. A modern attribute
without the exact opt-in version reports an error.

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

## Reproduce source and hosted checks

All compilation runs in Actions. The four existing full Linux profiles retain
release and strict ASan/UBSan/vptr/leak coverage plus the signature-enabled
Windows baseline. `rov_retail_xml` executes the production helper with actual
full-client Lua and XML nodes in both VM profiling modes. Authored Lua tables
exercise ordering, arguments, errors, opt-in, malformed inputs, native identity,
registry lifetime and ownership/taint. They do not establish actual frame
creation, rendering, secure-action parity, login or gameplay.

Supply a permitted export or a checkout of the exact reference outside both this
repository and game installations. For an existing external checkout:

```sh
git -C /external/wow-ui-source checkout 9dc84f0d3bbae1768ac3405b3716000bd93eda06
python3 -B Tools/LinuxClient/RetailUI/audit_reference.py /external/wow-ui-source \
  --report /external/edit-mode-dependencies.json
```

The audit refuses a different declared-graph fingerprint. Download the exact
`full-linux-evidence-*` artifact identified in #212, extract `RovRetailXMLCheck`
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
2. Blizzard one-bag UI: reuse 12340 container slots and item actions behind
   reviewed container/item bindings, then the genuine bag implementation and
   templates. Do not invent account/warband/reagent storage unavailable in 12340.
3. Enhanced unit frames: reuse qualified layouts/atlases and original unit events;
   adapt genuine frame implementations without exposing unsupported aura data.
4. Cooldown manager: audit actual Forever/retail sources and APIs first. A narrow
   player cooldown display can use backed 12340 spells; modern cooldown-set,
   charge/override/spec and secret-data APIs require explicit capability limits.
5. Character customisation UI: follow the [modern model workstream](MODERN_ASSETS.md)
   and an explicit mapping to supported 12340 choices. New races/options or
   server-persisted appearance fields need separately approved extensions.

Order follows shared infrastructure and available original data; none of these
steps authorizes importing the entire modern framework or changing the protocol.
