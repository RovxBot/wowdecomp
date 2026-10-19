# Native original Storm string prerequisites

Source reference: product main `5090a23e6fc46f572df301ac8f1a9a13b29c409b`.
This work selects original `Storm/Source/SStr.cpp`, `SOption.cpp` and
`soprintf.cpp` into the native core to unblock the actual Gx/Gxu shader/font
hashes, cache names and localized ordered text. It does not install a native
CGxDevice or export the original login UI. M3 remains open.

The existing 12340 string hashes, slash/case normalization, UTF-8 comparison
rules, allocation and `TSHashTable<..., HASHKEY_STRI>` remain the caller contract.
Byte case conversion/comparison uses the original default C-locale ASCII rules,
independent of the host locale; it does not replace the engine's Unicode rules
with a system locale or a new general Unicode library. Host `long` width does not
change fixed-width decimal conversion. Native negative limits use unsigned
arithmetic and explicit bit copies. Original Windows bodies remain selected
outside `WOWDECOMP_PORTABLE_RUNTIME`; no donor address or VERIFY claim is added.

The original ordered formatter retains up to 64 arguments/specifiers, explicit
`%N$` ordering, repeated arguments, integer/pointer/double classes and literal
`%%`. Native dispatch uses standard `va_copy`/`va_arg`/`va_end`, bounded C
`snprintf`, and converts the original `I64` spelling to `ll` while passing the
matching native `long long` type. Truncation returns bytes actually retained,
always leaving room for the terminator. Regular bounded formatting uses host
`vsnprintf` with the same retained-byte return convention. In ordinary bounded
and unbounded formats, original `I64` integer directive lengths map to the host
length for `int64_t` (LP64 `long` or LLP64 `long long`); literal `I64` text and
escaped percent signs are preserved. This covers existing achievement and GUID
link/chat callers without enabling global ordered formatting.

Native ordered formats reject argument indices outside 1..64, gaps, conflicting
argument classes, overflowing numeric fields, excessive specifier lengths and
unsupported conversions before fetching missing arguments. An invalid ordered
format returns an empty string and sets the Storm invalid-parameter error.
All argument gaps are checked before any variadic fetch. The native parser
requires ordering before flags/width/precision/length and a matching conversion;
misplaced markers cannot discard a length modifier before libc dispatch.
The public `SStrVPrintf` preserves its caller's argument list in both ordinary
and ordered modes, including the original unbounded entry point.
Ordered UTF-16 `%S`/`%C` are explicitly unavailable pending an encoding adapter;
they are not reinterpreted using Linux's 32-bit `wchar_t`. Star width/precision,
single-`l` and `%n` remain outside the original ordered dialect. Ordinary host
wide-string formatting does not qualify original UTF-16 callers. The legacy
unbounded string APIs still require caller-owned sufficient storage; this work
is not a qualification of every original Storm API or every malformed UTF-8
sequence. Global option changes belong to startup, before concurrent consumers.

## Validation

All compilation runs in Actions. `CoreServicesCheck` now exercises the original
hash-table key allocation, growth, mixed-case lookups, deletion and cleanup for
three cycles; original path/hash and UTF-8 cases; fixed-width integer limits;
ordered mixed integer/string/double arguments, repeated 64-bit arguments,
`va_list` reuse, exact bounded truncation and invalid formats with no arguments
to fetch. Existing Linux, strict sanitizer, actual ARM64 macOS and Windows core
jobs cover the native ABI; full Windows client compilation covers the separately
selected legacy bodies. Hosted qualification is pending.

The initial `cdba9ff59010e753f9da7df6b01d545fdcb68044` implementation passed
all nine PR workflows. Follow-up source review found that a later missing
argument slot was rejected only after earlier variadic fetches, and misplaced
ordering markers could drop a length modifier while formatting. Native parsing
and full argument preflight now reject those cases; focused hosted fixtures also
cover truncated modifiers, invalid grammar and ordinary public `va_list` reuse.
Caller review also found original `I64` achievement/GUID formats in ordinary
mode; native directive translation and signed/unsigned high-bit fixtures retain
their fixed-width values without changing the default global option.
These changes need fresh hosted qualification; the earlier pass does not qualify
the corrected source. Local Python checks passed (32 passed, one unavailable
CMake check skipped); no client or dependency compiled locally.

No original game data, credentials, server requests or dependency downloads are
added. The [legacy Gx audit](LEGACY_GX.md) remains the next integration boundary.
Original font GPU evidence remains attached to PR #24's implementation and
[server revision discrepancies](SERVER_IDENTITY.md) remain preserved.
