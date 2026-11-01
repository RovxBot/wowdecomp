# Native login integrity boundaries

The original login UI can display an unavailable-service error and accept local
input while authentication remains refused. UI visibility, scan permission,
archive authentication and server integrity are separate state. Track the native
ScanDLL requirement in [#131](https://github.com/RovxBot/wowdecomp/issues/131),
transport in [#97](https://github.com/RovxBot/wowdecomp/issues/97) and authentication
and Warden in [#98](https://github.com/RovxBot/wowdecomp/issues/98).

## Original requirements

| Boundary | Original implementation and requirement |
| --- | --- |
| Original data/UI | Native authenticated archive reads, `FrameXML_CheckSignature` with the original InterfaceKey, all ordered Glue visits and the mandatory post-creation digest in `Glue/Portable/Startup.cpp`. A scan failure cannot relax these gates. |
| Windows startup detector | `Glue/ScanDLLGlue.cpp` obtains the version/download, invokes a worker and publishes error/completion on idle. `WowSvcs/WowSvcsClient/CheckExecutableSignature.cpp` verifies the executable with its original RSA key before loading the Windows library and calling scan ordinal 3 or 1. Only a real detector return produces OK/HACK/TROJAN and the block flag. |
| Login permission | `CGlueMgr::LoginServerLogin` checks `m_scandllOkayToLogIn`, TOS, EULA and idle state before calling `ClientServices::Logon`. Windows `ScanDLLContinueAnyway` can set that permission after a failure or a nonblocking detection. Native UI presentation must not inherit that permission. |
| Authentication proof | `WowServices/GruntLogin.cpp`, `Grunt.cpp` and the platform version-hash implementation are separate from ScanDLL. They must retain the actual 12340 challenge/proof protocol. Archive or DLL signature success is not a version proof. |
| World-session integrity | The original Warden message/module lifecycle is another requirement. The current Windows x64 `Game/GameClient/Win64/WardenClient.cpp` receives messages but does not implement complete module execution/response. It supplies no qualified native solution. |

Unchanged 12340/enUS `AccountLogin.lua` supplies a detector DLL URL only when
`IsWindowsClient()` is true. Its `SCANDLL_ERROR` handler hides the dialog, calls
`ScanDLLContinueAnyway()` and shows `AccountLoginUI`; the handler does not itself
authenticate. Its login action calls `DefaultServerLogin` and then clears the
password field. These call relationships establish a UI/error path, not a Linux
detector policy. Original Lua/XML stays unchanged and authenticated.

The [pinned Apple review](APPLE_STARTUP_REVIEW.md) finds a constant-true query
and an omitted Windows scan start. That port does not establish a native detector
result or unchanged-server integrity compatibility. No such source is imported.

## Native failure disposition

The Linux script-service owner has only NotStarted, ErrorPending and Unavailable
scan states. None approves sign-in. Start validates the two-string API and
defers one failure event to outer idle. Starts during delivery cannot reenter;
shutdown cancels pending delivery. A later start can present the failure again
when the original UI revisits agreements.

`ScanDLLContinueAnyway` leaves that failure and `IsScanDLLFinished()==nil` intact.
The original error handler may finish displaying its controls. The owner then
opens the original OKAY dialog with the actual unavailable reason.
`DefaultServerLogin` refuses every request before reading or retaining credential
bytes, including direct Lua calls, pre-start calls and calls after acknowledgement
or repeated continuation. Refusal notices are deferred to outer idle, after input
callbacks and the original login action's password-field clearing. Repeated
requests coalesce without retaining account/password bytes. Agreement state is never changed by these operations.
`StatusDialogClick` closes only the dialog owned by this failure path. There is no
transport/session owner or successful scan result in this implementation.

This is a supported failure presentation, not a native scanning capability. No
replacement detector policy has been established. A later implementation must
provide source-backed platform requirements and actual supported checks before
adding an approval state or connecting sign-in. Do not relabel a data digest as
the detector result, pretend to be another platform, omit a version proof or
ignore a Warden request to obtain a session.

## Server compatibility limits

Read-only source inspection of AzerothCore at
`d6f96d206b0e835fe692ebf4206e8db8a52d4d38` finds platform-specific Windows/Mac
version hashes in `AuthSession::VerifyVersion` and OS-dependent Warden selection
in `WorldSession::InitWarden`. Changing the claimed OS can change which integrity
checks a server applies; it is not a native compatibility fix. This source
checkout does not establish the running GrimGuzzler binary or policy. Preserve
the [live identity distinction](SERVER_IDENTITY.md), inspect current requirements
and observe an interactive session when the required native services exist.

## Validation

The existing D3D9 client contract executable exercises failure/error ordering,
repeated starts, login refusal before/during/after failure, original agreement
state, acknowledgement, owner/reentry guards, cancellation and recreation. It
uses authored data with no network. Actual original-data execution must separately
establish visible controls, input and shutdown; neither result qualifies a scan,
authentication or gameplay. All compilation runs in Actions with required Windows
regression and strict sanitizer checks. Runtime evidence stays in the issues.
