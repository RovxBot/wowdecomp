# Native 12340 packet prerequisites

This package prepares the existing `CDataStore`, ARC4 and SHA1/HMAC code for
native `WowConnection` integration. It does not implement sockets, DNS, connection
dispatch, SRP authentication, realm selection or a server session. M3 remains open.

## Source and caller audit

Source reference: merged main `365016e226547f7f3867f419988ccdf6b3182f25`.
No donor implementation, Cataclysm JAM schema, new dependency or asset is imported.
Original OFFSET/VERIFY comments identify the original 12340 source and its
unchanged Windows paths. They do not verify the guarded native adaptations.

| Existing boundary | Native adaptation |
| --- | --- |
| `Base/CDataStore.cpp`, `CDataStoreCache` | Explicit little-endian scalar loads/stores, including IEEE binary32; no aligned typed access to packet bytes. Preserve failed-read cursors and borrowed ownership. Reject wrapped write/read/array spans, growth of borrowed buffers and invalid borrowed input; retain allocation ownership on failure. Self-append retains its source across growth. |
| `Storm/Source/SArc4.cpp` | Use the original byte-wise scalar cipher transition at every alignment. Preserve key schedule, state copying and chunked output; reject an empty native key. This retains a legacy protocol requirement, not a recommendation to use ARC4 in a new protocol. |
| `BlizzardCore/.../Util/SHA1.cpp`, `WowServices/HMAC.h` | Retain original digest/HMAC algorithms. Native SHA1 compression copies the context API's input to aligned words; class streaming never forms a pointer before the input. Byte assembly uses unsigned arithmetic; interleaving scratch uses owned native memory. |

Native serialized fields keep their fixed widths. Runtime objects retain host ABI;
the code does not impose original Windows packing on pointers, locks or classes.
The selected Windows client bodies remain behind the existing non-native paths.
The HMAC fixture uses a 20-byte key and retains the existing template's
at-most-64-byte key precondition. It is not an observed authentication exchange
or a new general-purpose cryptography interface.

`WowConnection` requires more than the Os IOCP transport. Its current Win64
implementation performs partial `send`/`recv` and framed header encryption, while
`WowConnectionNet` uses `WSAEventSelect` and worker dispatch. Native work must adapt
both. Its packed `WORKER` embeds thread/event/lock objects, and shared connection
state uses Windows atomics; those require host alignment and explicit synchronization.
This packet target deliberately cannot claim that transport work is complete.

## Validation contract

All client/dependency compilation runs in
[packet Actions](../../.github/workflows/m3-packets.yml), through separate
`linux-packets`, `linux-packets-asan`, `macos-arm64-packets` and `windows-packets`
presets. The full client remains disabled. The graph audit permits only the three
packet sources and their semantic executable in addition to the existing core
subset. Actual ARM64 host/runner identity is required for macOS qualification.

`NativePacketCheck` checks fixed 12340 little-endian bytes, unaligned scalar/array
access, truncation, overflow rejection, borrowed/cache/detached ownership and
self-append through allocation. It checks the public ARC4 `Key`/`Plaintext` vector,
chunk/state-copy parity, SHA1 `abc`, HMAC-SHA1 `Hi There` and unaligned compression
parity/input preservation. All three lifetime cycles run with ASan/UBSan and leak
detection enabled in the Linux sanitizer configuration. Ordinary CI uses authored
bytes only, with no game data, credentials or server connection.

Qualified implementation: `6d1a9c209410d42f4351e71645c44edbe891cfb2` in
[PR #25](https://github.com/RovxBot/wowdecomp/pull/25). The
[durable record](packet-validation.json) preserves all nine successful PR
workflows, all four native artifact digests/manifests and five CTest passes per
configuration, including three packet cycles each. The macOS runner and Mach-O
binaries are ARM64. Linux ASan/UBSan/leak checks passed without suppressions.
Full Windows x64 compilation passed at separate merge checkout
`9a2bcc9d247cb93361dcc3fc26bbc8c85dc5ddff`, with signatures enabled and all
three packaged PE AMD64 binaries hash-verified. All 19 packaged binaries across
the five artifacts were verified against their manifests and archive digests.

Local checks discovered 33 Python tests: 32 passed, one CMake selection check
was unavailable locally. Documentation/JSON/whitespace checks passed. No client
or dependency compiled locally. These CPU fixtures do not establish sockets,
authentication, a new Windows gameplay session or native macOS desktop behavior.
The [external server revision discrepancy](SERVER_IDENTITY.md) remains unchanged.

The first implementation `ca01de61500b235b619a5942b51210e729730d12`
failed Clang compilation because the cache `DetachBuffer` declaration named a
parameter `size`, shadowing its template parameter, in
[run 39179683191](https://github.com/RovxBot/wowdecomp/actions/runs/39179683191).
The declaration now uses the definition's existing `size_` name; no ABI or
runtime behavior changes follow from this spelling correction.
