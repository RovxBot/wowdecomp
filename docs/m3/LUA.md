# Product Lua runtime prerequisites

This selects the product-modified Lua 5.1.1 and original Storm pool allocator
into a separate native ABI target. The caller is
`FrameScript_Initialize` in `FrameScript/FrameScript.cpp`: its three-argument
`lua_newstate`, taint globals, per-addon memory/time accounting, profiling VM,
and base/string/table/math/bit libraries are required by original UI/addon code.
A system Lua library does not provide that contract. This work does not yet
select FrameScript, Frame factories, XML, interface signatures or GlueXML/login.

`WowNativeLua` explicitly selects 25 original C sources as C++, matching the
Windows source-language choice, plus the original pool, original Base UTF-8 helpers and a native Os callback
validator. It excludes the host IO, OS, package loader and debug libraries that
FrameScript does not open. No system Lua, donor import, game data or new optional
dependency is used. Copyright notices and original Windows source selection
remain. Profile clock types use `int64_t` instead of the MSVC spelling; original
VERIFY/OFFSET comments retain their historical scope, not a new native claim.

Native numeric conversions use explicit bounds and the Windows x64 signed
indefinite result for nonfinite/out-of-range inputs. Bit shifts retain 32-bit
wrapping, x86 count masking and the original signed logical-right-shift result.
The native integer-remainder overflow case reports a protected Lua error.
Base-number parsing saturates at the Windows 32-bit unsigned-long limit,
including negative inputs; integer string formatting also retains 32-bit values
on LP64, the product's ordered argument grammar and modified UTF-8 helpers.
Original Windows branches remain selected outside the native runtime.

Callbacks retain the original executable-image restriction. Linux checks main
ELF executable load segments; macOS checks executable Mach-O segments in image
zero; native Windows checks executable PE sections in the main module. Null,
heap/data and shared-library/JIT callbacks are not admitted. The cached image
ranges live for the process lifetime. This adapts the original PE text check;
it does not silently accept arbitrary callbacks. The legacy Windows validator
remains selected by the full client.

Lua and its process-global taint/profile state belong to a single owner thread.
The caller must finish scripts, callbacks and GC before `lua_close`, then free
the pool. Pool address membership uses unsigned address arithmetic on native
targets; a recoverable cross-pool allocation failure retains its old allocation.
This prerequisite does not yet supply an application owner or adapt FrameScript's
file/log/UI services. The product always loads through `luaY_parser`: emitted and foreign bytecode
remain rejected, even though `lua_dump` is exported. Native loading preserves
this source-only behavior; it does not enable generic Lua undumping.

## Next original callers

Source inspection identifies `FrameScript_Initialize`'s compatibility Lua and
extra script registrations, `FrameScript_Object` event dispatch and listener
mutation, nested secure handlers and error recovery as the next consumers.
`FrameScript_ExecuteFile` and `FrameScript_LoadVariables` currently own legacy
`SFile::ReadFile`/`Unload` bytes; adapt their retained native reads and saved
variables separately. Keep original TOC digest/signature ordering when FrameXML
loads source. Variable serialization uses `OsWriteFile` and must retain partial
write/error behavior and external writable roots. `FrameScript_Destroy` closes
Lua before freeing its pool; Frame objects/listeners must finish before it.
`FrameScript_Flush` recreates the state and requires explicit owner lifecycle
coverage. This caller audit is source inspection, not new runtime evidence.

## Validation

All client/dependency compilation runs through
[M3 Lua Actions](../../.github/workflows/m3-lua.yml), with Linux release and
strict ASan/UBSan/leak detection, actual ARM64 macOS and Windows x64 native
fixtures. The generated graph audits exact sources, target ABI and the full
client gate. The existing Windows baseline remains the full-client regression.
Each package retains the original Lua header and complete license in
`notices/lua.h`, with a manifest hash.
Implementation `33ef88fdd742b6b2a4484f3c86c013642f977837` is qualified in
[PR #29](https://github.com/RovxBot/wowdecomp/pull/29), merged as
`f1726d9628bd15d09f8acee4442fff5976dac81f`. The
[durable record](lua-validation.json) retains all ten passing implementation PR
workflows, six CTest passes per native configuration, four verified archives,
16 native executable hashes/types and license hashes. Actual ARM64 execution is
corroborated by the runner and Mach-O identity. Full Windows x64 compilation used
separate matching-tree checkout `3c48f75f4e9bbe94a4e5c09c612fb9c88652acb3`, with
signature checks enabled; its archive and three PE x64 binary hashes were verified.
Downloaded exact-source Linux release and strict sanitizer executions passed six
Lua cycles each. Their six allocation-overflow diagnostics are expected fixtures;
no sanitizer finding occurred. No local client/dependency compilation ran.

Local Python source/policy checks: 38 passed, one existing unavailable-CMake
check skipped. JSON, documentation references and whitespace checks passed.
Original-data graphics qualifications remain attached to their own sources;
this package changes no selected Gx/Gxu implementation and records no new GPU,
physical-input, Windows runtime, macOS desktop or server acceptance.

 The initial link failure omitted
original UTF-8 helpers and the selected native fatal reporter. A later semantic
fixture incorrectly expected bytecode reload; source inspection confirmed the
product source-only parser, and the fixture now requires that rejection.
Strict UBSan then exposed null union-member formation at an empty upvalue-list
terminator and signed `INT_MIN - 1` in numeric table bounds. Native branches now
check the null terminator before member access and subtract in unsigned space;
original Windows paths remain. These failures are retained as failed evidence.

`NativeLuaCheck` runs three cycles each with profiling disabled/enabled. It
checks pool size-class moves and multiple chunks, allocation failure at state
creation and the first 40 partial-state allocations, failed cross-pool growth
with retained bytes, C callbacks, closures, metamethods, libraries, coroutines, protected errors, syntax and bytecode rejection, instruction-hook abort/recovery, taint propagation, addon allocation accounting and profile call
counts. Integer-width, ordered-format and bit-edge fixtures are included.
Separate child processes must reject null/data/heap callback addresses
with the original fatal diagnostic. These are authored fixtures without game
data or original login execution. Complete secure-environment/taint callback
behavior remains to be qualified as FrameScript is adapted. Allocation-overflow diagnostics are expected authored
failure injections, separately from sanitizer findings. No detector is disabled.

The next consumers are FrameScript's script/event registration, secure handlers,
file/log services and original Frame/XML factories. Original TOC signatures must
be adapted separately from MPQ signaturefile authentication. File-backed images,
SDL size invalidation, networking/authentication and observed gameplay also
remain open. M3 is incomplete; the user will sign in interactively when the
actual original login UI is ready.
