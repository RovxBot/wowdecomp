# Original FrameScript native integration

This follows the [product Lua prerequisite](LUA.md) and selects the original
12340 FrameScript, FrameScript_Object and script helper sources into a separate
native ABI target. It adds no host Lua, donor import or optional dependency.
Historical OFFSET/VERIFY annotations describe the original Windows bodies;
native branches below are adaptations, without a new binary verification claim.

`FrameScript::Native::Runtime` owns the process-global VM on one thread. Public
FrameScript entry points and object/cache mutations check that owner before
mutation. Finish scripts and destroy FrameScript objects and external function
references before flushing or closing the VM. Nested script execution and live
objects prevent Destroy/Flush; finalizers run before pool release. A second VM
owner is rejected. Borrowed Lua state access is confined to this same owner and
must not escape its lifetime. The prerequisite Lua target remains usable on its
own under its separate caller contract.

The source provider returns completed, retained worker reads and distinguishes
interface source from saved variables. Interface reads must use original-key
authenticated packages with registered-MD5 verification; saved variables use an
external writable user root. There is no missing-MPQ fallback to host files.
The adapter normalizes relative MPQ-style names before the provider, rejects
root escape/absolute/drive paths and names of 260 bytes or more, and bounds each
completed source to 8 MiB. Empty source is valid. Read/provider failures report
failure through the original status boundary. File traversal feeds original MD5
bytes before execution. Original TOC/RSA interface signature traversal is separate
FrameXML work; this file provider does not qualify interface signatures.

The original saved-variable environment, SavedVariables taint, serializer,
script registrations, compatibility aliases, securecall/hooksecurefunc and
event globals remain selected. Native writes retry partial progress and report
failed or zero-progress writes. Nested event dispatch defers listener changes
until the outermost signal; object teardown removes registrations and invalidates
the Lua object table before VM destruction. Native error handlers recover their
mode after a failing user handler. The native helper branch bounds replacement
copies, rejects empty replacement searches and preserves Windows integer widths
without undefined nonfinite conversions. Original Windows paths remain selected
for the regression client.

The [Actions workflow](../../.github/workflows/m3-framescript.yml) builds release
and strict ASan/UBSan/leak Linux fixtures, actual ARM64 macOS and native Windows
x64 fixtures. It audits the exact source graph, retains the bundled Lua license,
and packages hashes and detailed CTest output. The existing Windows baseline is
the full-client compilation regression. All compilation is hosted; local Python
source/policy checks are permitted.

`NativeFrameScriptCheck` exercises actual public entry points with authored
scripts and retained FileQueue host reads: compatibility helpers, secure calls,
hooks/results, taint, syntax/runtime/handler recovery, nested listener mutation,
legacy this/event/arg restoration, profile counters, object invalidation, foreign
thread rejection, digest ordering, read failures, saved-variable round trips and
write failures, then repeated Flush/Destroy with profiling off/on. This is not
original GlueXML execution, GPU rendering, physical input or server acceptance.
Hosted results and exact identities will be recorded after qualification.

Next are original Frame/XML factories and file-backed image/async Services,
interface signatures, GlueXML startup and actual original login rendering.
Networking/authentication, character selection and observed GrimGuzzler world
entry follow. M3 remains open. The user will sign in interactively when ready;
signatures and the external server revision discrepancy remain unchanged.
