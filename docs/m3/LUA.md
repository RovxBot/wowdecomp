# Product Lua runtime prerequisites

This selects the product-modified Lua 5.1.1 and original Storm pool allocator
into a separate native ABI target. The caller is
`FrameScript_Initialize` in `FrameScript/FrameScript.cpp`: its three-argument
`lua_newstate`, taint globals, per-addon memory/time accounting, profiling VM,
and base/string/table/math/bit libraries are required by original UI/addon code.
A system Lua library does not provide that contract. This work does not yet
select FrameScript, Frame factories, XML, interface signatures or GlueXML/login.

`WowNativeLua` explicitly selects 25 original C sources as C++, matching the
Windows source-language choice, plus the original pool and a native Os callback
validator. It excludes the host IO, OS, package loader and debug libraries that
FrameScript does not open. No system Lua, donor import, game data or new optional
dependency is used. Copyright notices and original Windows source selection
remain. Profile clock types use `int64_t` instead of the MSVC spelling; original
VERIFY/OFFSET comments retain their historical scope, not a new native claim.

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
file/log/UI services. Bytecode round trips use the current native ABI; foreign
headers are rejected. No claim of 32-bit retail bytecode interchange is made.

## Validation

All client/dependency compilation runs through
[M3 Lua Actions](../../.github/workflows/m3-lua.yml), with Linux release and
strict ASan/UBSan/leak detection, actual ARM64 macOS and Windows x64 native
fixtures. The generated graph audits exact sources, target ABI and the full
client gate. The existing Windows baseline remains the full-client regression.
Qualification is pending these hosted runs.

`NativeLuaCheck` runs three cycles each with profiling disabled/enabled. It
checks pool size-class moves and multiple chunks, allocation failure at state
creation, C callbacks, closures, metamethods, libraries, coroutines, protected
errors, syntax/header rejection, native bytecode round trips, instruction-hook
abort/recovery, taint propagation, addon allocation accounting and profile call
counts. Separate child processes must reject null/data/heap callback addresses
with the original fatal diagnostic. These are authored fixtures without game
data or original login execution. Full bit-operation edge semantics and complete
secure-environment/taint callback behavior remain to be qualified as FrameScript
is adapted. No detector is disabled.

The next consumers are FrameScript's script/event registration, secure handlers,
file/log services and original Frame/XML factories. Original TOC signatures must
be adapted separately from MPQ signaturefile authentication. File-backed images,
SDL size invalidation, networking/authentication and observed gameplay also
remain open. M3 is incomplete; the user will sign in interactively when the
actual original login UI is ready.
