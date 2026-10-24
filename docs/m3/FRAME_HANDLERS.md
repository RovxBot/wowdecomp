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

## Qualification

[PR #74](https://github.com/RovxBot/wowdecomp/pull/74) is qualified at
`9d3384f9436aaa9c763fca0c35a8a91b1be734b9`, merged as
`1cea05886d4fa4b68552f1615030e18ce7de111b`. [Durable validation](frame-handler-validation.json)
records final-head hosted GL release/strict ASan/UBSan/leak configurations passing
38 CTest checks each. Twelve other PR workflows passed at
`ac2b05f84ab00920244b987096ed59d923fd8e9e`, including full signature-enabled
Windows x64 compilation at checkout `a2e991897220e6ad51bea9193cc85866a7c06929`
and actual ARM64 macOS native prerequisites. Only two GL-only fixtures and
scheduling documentation changed afterward; the other 3,876 tracked files are
identical. Preserve those earlier workflow identities; they are not final-head
runs. The final GL checkout, selected implementation and merge share one tree.

Three archive digests and all 51 packaged executable hashes/types/notices verified.
Downloaded final-source RX 7900 XTX / Mesa 26.2.5 / SDL X11 release and strict
sanitizer packages passed 30 authored/original-data desktop executions, three owner
cycles each, with empty stderr and zero workers. Four resource fixtures passed
three cycles per mode. All fifteen capture pairs match. Final original-data input/
hierarchy captures retain SHA-256
`55b0e2c731d888293851e66a21b2b7eccaa29405097fb4b6b1e65588f60c93cf`.
The inspected final capture remains authored hierarchy text; intermediate readbacks
require distinct A, V and AV pixels from actual key/update Lua callbacks.

Earlier fixture failures remain recorded: original issecure returns nil rather
than false, seterrorhandler accepts functions although the default is nil, and
an older mouse fixture expected the previously unavailable callback to reject.
The tests now retain an execution witness, restore the prior registry reference
exactly and invoke a real mouse callback. Product API behavior and sanitizer
detectors stay unchanged. The historical Actions-disabled dispatch and subsequent
user re-enablement remain separate evidence. Local source/launcher checks:
57 passed, one unavailable-CMake skip; Windows token/extraction and whitespace
checks passed. No client/dependency compiled locally.

[Scheduling PR #75](https://github.com/RovxBot/wowdecomp/pull/75) removed duplicate
feature-branch push builds across thirteen workflows, retaining PR/main/manual
checks and all jobs/configurations. Its trigger-only change passed YAML semantic
comparison, local source checks and hosted M0 audit. The fixture-only follow-ups
reran GL with verified unchanged production inputs, avoiding repeated full
matrices; see [build scheduling](../CI_BUILD_POLICY.md).

Remaining widgets and broader structural callback edits, physical IME/edit-box/
candidate acceptance, complete signed ordered GlueXML/full second digest gate,
actual login rendering and transport/gameplay remain open. No new physical-input,
Windows runtime, macOS graphics or server acceptance is claimed. Keep signatures,
server revision discrepancies, read-only clean data and interactive user sign-in
intact. M3 remains incomplete.
