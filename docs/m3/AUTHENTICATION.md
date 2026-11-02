# Original native authentication and session boundary

Current work and execution evidence live in [#98](https://github.com/RovxBot/wowdecomp/issues/98),
with transport [#97](https://github.com/RovxBot/wowdecomp/issues/97) and the
[integrity prerequisites](INTEGRITY.md). The operator has physically observed
verified live SRP and one advertised realm with a merged TEST-SERVER release;
[exact artifact and retained launch/shutdown provenance](https://github.com/RovxBot/wowdecomp/issues/98#issuecomment-6571573207).
That live result is separate from controlled fixtures. Live world authentication,
character selection, Warden handling and gameplay remain acceptance requirements.

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
It checks short opcodes and complete challenge bodies before processing. Original
12340 proof-of-work searches LE uint64 nonces in bounded Event slices (at most
1,024 attempts / 2 ms per poll), with a 30-second deadline. Invalid difficulty,
duplicate challenges, timeout, cancellation and disconnect retire pending work;
no worker retains the session. Unqualified redirect/suspend/resume controls and
Warden requests disconnect. World object-manager context remains unavailable.
The TEST-SERVER-only original `ClientServices`/`RealmConnection` selection below
extends this boundary through world authentication and character enumeration.

## Integrity gates and unsupported paths

In normal builds, `DefaultServerLogin` refuses every request without reading or forwarding
credentials. Production `GruntLogin::GetLogonMethod` cannot select a supported
native platform/version-integrity identity and reports failure without emitting a
logon request. No Windows/Mac identity is chosen as a native substitute. Identity
refusal erases pending credentials before the failure callback. No native
loaded-executable checksum inventory or detector approval is implemented here.
The supported file-mode checksum callback is distinct from that missing inventory;
`ChecksumExecutables` explicitly rejects its unsupported native inventory without
publishing output. The fixture alone overrides the identity callback with authored
values and feeds a temporary authored file through the original challenge-keyed
HMAC and native file callback. Independent Python HMAC bytes then verify the
Grunt version proof. It is not reachable from
Lua, startup options or the product sign-in path.

Reconnect and file-transfer authentication commands remain unavailable natively;
reconnect entry rejects instead of using the original zero-checksum path. Warden,
required production native platform/integrity behavior and the remaining
redirect/reconnect/full world lifecycle remain work under #98. External DNS shutdown limitations remain in [CONNECTIONS.md](CONNECTIONS.md).
No server/cluster/Warden policy, agreement, archive/interface key or signature gate
is changed by this integration. The separately compiled, explicitly activated
[isolated test-server profile](#isolated-test-server-profile) has a narrower,
operator-authorized capability disposition; it does not qualify production integrity.

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

Configure the isolated endpoint through the existing external settings root:
`WTF/Config.wtf` may contain `SET realmlist "<isolated-host>"`. Preserve the two
read-only installations and keep that file outside Git. This configuration grants
no integrity capability and does not connect the gated product sign-in path.
Normal builds remain refused until supported platform/version/detector/Warden
requirements justify a session. Never embed a test hostname in client code.

## Isolated test-server profile

This **test-only Linux x86-64** profile is tracked in [#98](https://github.com/RovxBot/wowdecomp/issues/98).
It connects the original `DefaultServerLogin` Lua API to the original
`GruntLogin`/SRP6/realm-list callers and Event transport. Verified authentication
hands the original session key to `ClientServices`/`ClientConnection`; a later auth
socket EOF does not invalidate that verified handoff. The original realm dialog
uses advertised names, addresses and flags. Saved realm preferences never supply
an endpoint. Refresh after auth EOF requires a new sign-in; reconnect stays refused.

The selected `RealmConnection` constructs original 12340 `CMSG_AUTH_SESSION`
(build/server identity, account, client seed, selected realm, work proof and
SHA1 account/seed/session-key digest), then enables original header encryption.
Actual server rejection, billing/expansion records, queue updates and character
responses drive the original owners/UI. Only AUTH_OK admits character requests;
bounded enumeration accepts up to ten complete original records and its optional
race restrictions. Malformed/unsolicited messages fail closed. No credentials,
keys, character names or private packet contents are logged.

Original character-screen scripts, scalar component assembly, authenticated DBC/
texture/model reads and display release are selected only in this profile. Rows
and model data come from the actual server and unchanged data. Attached resources
retire before their frame/scene owners; screen hide unregisters original idle.
Original character OnShow/button/update scripts retain bounded scalar requests;
the outer Glue poll applies model changes after Frame callbacks/traversal. Session
retirement discards pending controls before releasing their rows and resources.
Ghost lighting/effects and guild-tabard query/cache paths remain explicitly
unavailable. Native addon producers are not loaded: the auth-session field encodes
an empty loaded inventory, and actual server banned-addon records are bounded and
retained in-session without addon approval or data-tree writes. Character mutation,
creation and world entry remain refused. Runtime/resource acceptance remains #99;
source selection and fixtures do not establish a complete observed character screen.

Both gates are required:

1. Build in Actions with `WOWDECOMP_NATIVE_TEST_SERVER_LOGIN=ON`. It defaults
   **OFF**, is absent from normal presets and is forbidden for Windows, macOS,
   signature-disabled builds and archived OpenGL startup. Normal release builds
   exclude the implementation and reject the runtime flag. An optimized Release
   test artifact is still a separately identified **TEST-SERVER** artifact.
2. Launch that test artifact with `--test-server-login`. This is not a saved CVar,
   environment switch or Lua approval API. Omitting it retains credential refusal,
   including in a test-capable artifact. Closing/recreating Glue clears the allowance.

The Grunt identity is explicitly nonstandard Linux `Lnx` (`0x004c6e78`) and
x86-64 `x64` (`0x00783634`), with retail `WoW`, version 3.3.5/build 12340 and the
integrated target's authenticated enUS locale. FourCCs retain the original
little-endian serialization. This does not impersonate Win/OSX or claim that a
server supports the identifiers. The externally configured server must accept them.

ScanDLL remains unavailable: `SCANDLL_ERROR` is delivered, continuation only
displays controls, `IsScanDLLFinished()` stays nil, and no successful detector
event/approval is emitted. The visible test-profile notice must be dismissed
before login can start. Original TOS/EULA decisions remain mandatory and are
never accepted automatically. Archive authentication, original InterfaceKey,
ordered Glue creation and post-creation digest remain unchanged.

Native loaded-executable integrity is also unavailable. In the protocol's required
version-proof field the profile sends the original `SHA1(A || checksum)` composition
with an explicit **20-zero-byte unavailable-checksum placeholder**. It never calls
`ChecksumExecutables`, reports a successful check or treats this value as a native
inventory/hash. This requires the isolated auth server's external
`StrictVersionCheck=0`; a rejecting server's bad-version/error response remains a
failure. SRP client proof, complete server-proof verification and bounded realm
parsing remain required. PIN/matrix/token, patch/survey downloads and reconnect are
unsupported and stop sign-in. Warden is unavailable; no Warden request is ignored
or answered with fabricated success. Only an actual worldserver AUTH_OK establishes
world authentication; reaching character selection grants no world-entry capability.

### Build and interactive test requirements

The operator reports GrimGuzzler is isolated, with Warden disabled and strict
version checking already disabled. This is distinct from the older mounted-setting
observations in #98. Confirm the running endpoint/policy externally before testing;
the client cannot attest server configuration and this work changes none of it.
Requirements are `StrictVersionCheck=0`, `Warden.Enabled=0`, acceptance of the
truthful Linux/x64 identity, a 12340-compatible account and reachable advertised
realm endpoints. Do not use this profile against an ordinary production endpoint.

Dispatch the existing Actions workflow with its explicit test input:

```sh
gh workflow run native-d3d9-client.yml --repo RovxBot/wowdecomp \
  --ref <reviewed-branch> -f configuration=both -f test_server_login=true
```

Download the separately named `native-client-d3d9-TEST-SERVER-*` release/strict
artifacts. Verify the Actions source SHA, artifact digest and every packaged file
against `manifest.json`; it must record `test_server_login: true`, required
signatures and the matching sanitizer mode. The launcher performs package hash
checks and rejects profile activation with a normal artifact.

Put only the endpoint in the external settings root's `WTF/Config.wtf`:

```text
SET realmlist "<isolated-host>:3724"
```

An explicit host is mandatory; there is no retail fallback. IPv4/DNS host names
and an optional port are supported by the existing transport. Keep settings,
cache, logs and reports outside both read-only reference installations. Then run:

```sh
python3 Tools/Porting/run_native_client.py --renderer dxvk --test-server-login \
  --artifact <verified-test-artifact> --data /mnt/games/3.3.5aClean/Data \
  --settings <external-settings> --cache <external-cache> \
  --output <external-report.json> --timeout 600
```

Use `--sanitizers` with the matching strict artifact. Accept original agreements
interactively if required, read/dismiss the capability notice, enter the test
account in the original fields and click Login. Cancel must retire the connection;
bad credentials/version/server proof must fail. Select an actual advertised realm
in the original dialog and record world-auth responses, character enumeration and
the original character screen separately. Enter World remains unavailable.
Record exact artifact/source/server/OS/GPU identities, visible outcome, cancellation
and release/strict shutdown separately in #98, without account, password, session
key or private packet captures. The external-DNS shutdown bound (#97) and hardware
strict exit-leak gap (#147) remain separate requirements. The recorded operator
SRP/one-realm observation does not establish live world authentication or character
selection; those require the updated matching packages and further operator input.
