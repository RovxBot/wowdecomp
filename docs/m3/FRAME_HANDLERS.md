# Original Frame Lua and XML handlers

This package selects original 12340 Frame script discovery, GetScript/SetScript/
HookScript, event subscriptions and keyboard/mouse enablement. Original callbacks
now execute through the qualified product FrameScript VM from the original root
queues and Frame load/visibility/size/update/mouse/drag consumers. The unchanged
original mouse-button converter moves into a shared source without selecting the
unadapted Button class. Windows-selected implementation tokens stay unchanged.
All client/dependency compilation runs in GitHub Actions.

Native SetScript/HookScript retain argument 3 even with trailing arguments. Hooks
keep original wrapper order and taint behavior; replacing/unsetting an executing
handler releases its registry reference while Lua retains the active closure.
Frame events keep original pending subscription semantics. Callback errors use the
original protected error handler. Internal compatibility globals (`this`, `event`,
`argN`) use native raw writes so global assignment metamethods cannot escape the
protected call; their prior values restore after execution.

Frame callbacks guard structural changes and destruction through the existing
resize/traversal admission. Script references/subscriptions and selected FontString
text setters remain usable inside callbacks. Text changes dirty original string
caches before the next batch collection; batch collection/drawing still reject
text edits. This first bounded package does not qualify structural edits from
load/update/input callbacks. Broader widget/Lua behavior remains ahead.

Frame/Scripts XML accepts original base Frame handlers, inline source and named
global functions. Compiled wrappers live in XMLNodeData on retained virtual nodes;
each Frame slot independently retains its function after template cleanup. Native
allocation uses matching new/delete, syntax failures leave an empty slot and
status, and nonfunction globals are unavailable. Preflight bounds inline source
and rejects unknown handlers, nested script children and unsupported inherit
modes before Frame allocation. This is not the ordered TOC/Include/Script executor
or the complete post-creation interface signature gate.

The hierarchy fixture exercises live canonical identities, hook order and trailing
arguments, self replacement/unset, original root key/Unicode character delivery,
mouse/drag arguments, update-driven text pixels and exact repeats, visibility,
event subscriptions, callback errors/recovery, taint, internal-global assignment
hooks, compiled template reuse/cleanup, invalid XML and teardown. Authored input
injection remains separate from physical keyboard/IME/edit-box acceptance.
Original-data runs retain read-only clean 12340/enUS resources, original-key MPQ
verification, registered MD5 checks and before/after GlueFonts-subset InterfaceKey
checks. Complete signed ordered GlueXML, widgets/edit boxes, actual login,
transport/authentication and observed gameplay remain open. Preserve the server
revision discrepancy and interactive user sign-in. M3 remains incomplete.

[PR #74](https://github.com/RovxBot/wowdecomp/pull/74) is an unqualified candidate
at implementation `e6af780f68dae599cf744769092f9d9cd184756f`, following merged
PR #72/closeout #73. [The validation record](frame-handler-validation.json) retains
local checks and the external hosted-build block. Local source/launcher checks:
57 passed, one unavailable-CMake skip; Windows-selected tokens, original helper
extraction and whitespace passed. No client/dependency compiled locally.

No automatic push/PR workflows appeared. An explicit M3 GL workflow dispatch
returned HTTP 422, `Actions has been disabled for this repository.` Read-only
repository queries simultaneously reported Actions permissions enabled, the
workflow active and the repository neither disabled nor archived. The specific
account/repository restriction is not established. No repository policy was
changed, no hosted or downloaded implementation binary was produced, and no
implementation/runtime pass or new physical-input/server acceptance is claimed.
The PR remains unmerged pending hosted release/strict sanitizer and full Windows
compilation, verified artifacts and original-data RX 7900 XTX execution.
