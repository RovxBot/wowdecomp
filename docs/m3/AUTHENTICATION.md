# Original native authentication and session boundary

Current work and execution evidence live in [#98](https://github.com/RovxBot/wowdecomp/issues/98),
with transport [#97](https://github.com/RovxBot/wowdecomp/issues/97) and the
[integrity prerequisites](INTEGRITY.md). No live native authentication, realm
observation, Warden response or gameplay is qualified by the controlled fixtures.

## Original callers and ownership

The native connection target selects this checkout's original `Grunt::ClientLink`,
`GruntLogin`, `Login`/`LoginResponse`, SRP6/BigInteger/Storm SBig and `NetClient` /
`NETEVENTQUEUE`. No donor implementation, game data or new dependency is imported.
Original address/VERIFY annotations describe the Windows bodies; they do not
verify the guarded native adaptations. Runtime objects use host alignment/widths;
packet fields retain the original 12340 encoding.

Event polls sockets first, then `NativeSession::Poll` pumps the Grunt timer and
NetClient queues before normal update/idle/paint. Native Grunt starts no timer
thread. Session creation, polling and destruction require the same transport owner;
sessions must be destroyed before Event/transport shutdown. Snapshot identities
protect against deletion/recreation by an earlier callback. Grunt callbacks may
release their login owner; a separate lifetime token prevents outer command parsing
from touching its retired datastore. NetClient event payloads and client references
are released on consumption, exceptions and cancellation. `DelayedDelete` defers
client retirement through the original event queue, including callback requests.
Direct destruction/queue clearing and nested polling during queue dispatch reject.
The last NetClient does not destroy the Event-owned transport.

## Challenge, proof and realm-list contracts

The original stream command handlers accept fragmented challenges and proofs.
Native parsing checks command state, bounds retained input, retains incomplete
commands and compacts the tail without overlapping `memcpy`. A complete logon
challenge calculates original SRP6 proof using secure random bytes, publishes
PIN/matrix/token requirements, then waits for the caller's version checksum.
Native SRP rejects invalid pointer/length inputs before constructing big numbers;
malformed matrix dimensions and unknown security flags fail the command.

A new-session proof cannot be emitted without the complete challenge and a supplied
checksum. Server proof must verify before a session key or authenticated status is
published. A partial success response waits for all proof, account, survey and
login-flag fields. Realm requests require that verified state. Realm responses are
parsed and delivered inside their declared length, so another coalesced command
cannot complete a truncated realm or confuse `LoginResponse::HandleRealmData`'s
end-of-message check. Original realm names, advertised addresses, populations,
flags, character counts and optional build fields pass through the original
consumer; saved CVars do not replace advertised endpoints.

Native NetClient receives original framed world messages, queues connect/data/
disconnect/auth-challenge events and dispatches original opcode handlers on Event.
It checks short opcodes and complete challenge bodies before processing. A nonzero
proof-of-work difficulty currently disconnects: unbounded work on the Event owner
is unsupported. World object-manager context remains explicitly unavailable.
This is not a completed `RealmConnection`/`ClientServices`/Warden world session.

## Integrity gates and unsupported paths

`DefaultServerLogin` still refuses every request without reading or forwarding
credentials. Production `GruntLogin::GetLogonMethod` cannot select a supported
native platform/version-integrity identity and reports failure without emitting a
logon request. No Windows/Mac identity is chosen as a native substitute. No native
executable/version checksum producer or detector approval is implemented here.
The fixture alone overrides that virtual identity callback with authored values
and supplies a checksum over authored executable bytes. It is not reachable from
Lua, startup options or the product sign-in path.

Reconnect and file-transfer authentication commands remain unavailable natively;
reconnect entry rejects instead of using the original zero-checksum path. Warden,
required native platform/integrity behavior, actual world auth-session construction,
nonzero world proof-of-work scheduling and the full session lifecycle remain work
under #98. External DNS shutdown limitations remain in [CONNECTIONS.md](CONNECTIONS.md).
No server/cluster/Warden policy, agreement, archive/interface key or signature gate
is changed by this integration.

## Controlled qualification

All compilation runs in Actions through `native-connections.yml` (Linux release,
Linux ASan/UBSan/leaks and actual macOS ARM64). `NativeSessionCheck` runs three
lifecycles. The same fixture runs through real Event Tick in the original D3D9
client contracts. Full signature-enabled Windows client compilation remains a
separate regression requirement. Frozen OpenGL code/tests are not extended.

`Tools/NativeNet/generate_auth_vectors.py` regenerates independent Python
`hashlib`/integer-`pow` SRP vectors in `auth_vectors.h`. The C++ fixture checks those
vectors and implements a loopback peer that derives its session from received A,
verifies the client and authored version proof, and returns the corresponding
server proof. Negative cases reject bad proofs and malformed messages. Fixture
credentials, platform identity, executable bytes and advertised realms are authored
and contain no private accounts or game data. Fixture authentication is distinct
from observing an interactive GrimGuzzler session or qualifying native integrity.
