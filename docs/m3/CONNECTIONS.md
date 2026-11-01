# Original native connection boundary

Current work and execution evidence: [#97](https://github.com/RovxBot/wowdecomp/issues/97).
Authentication, version proof and server/Warden requirements remain gated by
[#98](https://github.com/RovxBot/wowdecomp/issues/98) and native integrity
[#131](https://github.com/RovxBot/wowdecomp/issues/131). Transport availability
never grants sign-in permission; DefaultServerLogin still refuses credentials.

## Callers and ownership

Original `NetClient::Initialize` initializes `WowConnectionNet` and constructs
`WowConnection`; world packets use `SendPacket` / `WCMessageReady` and the existing
NETEVENTQUEUE. `Grunt::ClientLink::Connect` selects `WOWC_TYPE_STREAM`, default port
3724 and a 5000 ms timeout; `WCDataReady` feeds its existing command datastore.
The Battlenet caller also uses streams. These upper authentication/session owners
are not selected by the native startup build yet.

Native `EventInitialize` installs the same connection owner. Its first POLL
handler services DNS results and nonblocking POSIX socket readiness before the
normal file/image handlers, UPDATE, IDLE and PAINT. Responses use the original
`WowConnectionResponse` methods synchronously on the Event owner. Callers may
send, disconnect, replace/detach a response or release a connection in a callback.
Snapshot references and per-connection generations prevent deletion or stale fd
readiness during callback cancellation/reconnect. Nested network polling and
shutdown during dispatch reject; Event retains its destruction guards.

Only DNS runs off-thread. A bounded queue copies hostnames into independent jobs;
release/acquire publication returns the first IPv4 result. Workers never access
connections or client callbacks. Cancellation, timeout and reconnect retire old
jobs without publishing their results. Timeout includes lookup and connect.
`EventDestroy` closes all descriptors, clears queued I/O and joins DNS before
retiring the owner, without synthesizing shutdown callbacks. Surviving caller
references remain releasable but cannot reconnect through a retired owner.

## Packet and platform contracts

This adapts this checkout's 12340 `WowConnection.cpp` and Win64 methods; no donor
source/dependency is imported. Shared SENDNODE construction, CDataStore, SHA1/HMAC,
ARC4 key derivation/drop and response interfaces remain selected. Native send nodes
use aligned Storm allocations rather than Windows ObjectAlloc handles. Original
Windows bodies/ABI, WDataStore heaps and socket dispatcher remain separately selected.
Original OFFSET/VERIFY comments describe their original bodies, not native validation.

Message mode preserves two/three-byte big-endian lengths, ordered complete-frame
callbacks, partial headers/bodies and caller-selected encrypted opcode spans.
Stream mode preserves raw ordered bytes. Queued sends retain partial-write offsets;
EINTR retries, EAGAIN, EOF/errors and SIGPIPE handling stay explicit. Each poll has
bounded read/write work. Host NETADDR is a copied sockaddr_in, never a packed or
serialized native runtime structure. AF_INET preserves the original address API;
no protocol, keys, version identity, integrity result or server policy is invented.

## Qualification and remaining boundaries

All compilation runs in Actions. `linux-connections`, `linux-connections-asan` and
`macos-arm64-connections` exercise the existing core/packet prerequisites plus
`NativeConnectionCheck`. The same loopback fixture runs through actual Event Tick
in `NativeD3d9ClientCheck`. Fixtures contain authored bytes only and no credentials,
realms or game data. Full signature-enabled Windows compilation remains required.
Linux strict checks retain ASan, UBSan and leak detection without suppressions.

POSIX getaddrinfo cannot cancel an active libc lookup. Callback cancellation and
deadlines remain responsive while polling, but shutdown joins that lookup and can
wait for the system resolver. Controlled localhost fixtures do not establish a
bounded shutdown under an unavailable/misconfigured external resolver. IPv6,
listening/server transport, lag simulation and multiple network callback owners
are unsupported. Native NetClient/NETEVENTQUEUE, Grunt challenge/version proof,
realm/session/Warden integration and observed GrimGuzzler execution remain further
work. A hosted loopback pass does not establish authentication or gameplay.
