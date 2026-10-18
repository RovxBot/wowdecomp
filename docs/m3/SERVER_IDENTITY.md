# M3 live server identity observation — 2026-10-18

A read-only snapshot now records live GrimGuzzler auth/world container identities,
running executable hashes, the world binary's version-only output, mounted Warden
settings and listener/network observations. The exact server metadata, addresses
and raw evidence stay external in `server-identity.json`. Its SHA256 is retained
in the [render-target validation record](render-target-validation.json), alongside
the independent native GPU work package. No game client authenticated or entered
the world during this inspection.

Both auth/world application containers were ready at the observation. Their
immutable runtime image IDs were captured, and each PID 1 executable's hash
matched the corresponding packaged binary. The world binary's embedded revision
differs from the image package's source-looking tag and includes a `+` suffix.
Keep those identities separate: the tag does not establish the exact compiled
core source, a clean source tree or all module revisions. Do not substitute the
local deployment/source checkout revision for the observed executable identity.

The seven mounted Warden settings match the desired policy recorded in M0.
Reading that file does not prove which checks were negotiated, that a client
answered them, or that a native session survived the check/response window.
Listener/interface and configured realm-id observations do not verify the endpoint
advertised by authentication's realm list. That remains a session validation step.

The inspection used the existing Kubernetes context with explicit namespace and
resource selectors. It selected image/readiness fields and read only executable
hash/version, network metadata and allowlisted configuration/listener fields.
Source inspection established that the version-only invocation exits before
server/database initialization. No secret, account, whole configuration or game
data dump was collected; no cluster/server policy or deployment changed.
The no-row service query used an application-label selector and does not establish
that services are absent.

The M0 reports keep their original historical desired-state and operator-session
identities. This later live snapshot does not retroactively establish the server
used in those sessions or close M0's captures/session/Warden gaps. M3 still needs
the complete native client and an observed unchanged-data world session with
authentication, character selection, movement, zoning, combat/chat/inventory,
logout and Warden outcomes recorded against the then-current server identity.
