# M0 Windows runtime session record

Status: template; no runtime results recorded. Build the client in GitHub Actions,
then run the artifacts on a native Windows machine. Hosted build success does not
establish gameplay or GPU support. Keep private screenshots/logs and credentials
outside Git; record sanitized results and durable artifact identities here or in a
dated copy when the session is performed.

## Session identity

| Field | Observed value |
| --- | --- |
| Date/time and operator | Pending |
| GitHub Actions run and source commit | Pending; use a passing Windows baseline run |
| Artifact name and SHA-256 manifest | Pending |
| Windows edition/build, CPU, RAM | Pending |
| GPU and driver version | Pending |
| VC++ runtime/toolchain dependencies | Pending |
| Data source, build, locale, archive hashes/provenance | Pending; supplied ChromieCraft directory is modified and not stock-verified |
| Live GrimGuzzler core revision/image digest | Pending; deployment-source tag is recorded in BASELINE.md |
| Auth and realm/world endpoints | Pending verification; keep credentials external |
| Server Warden settings and observed checks/timeouts | Pending |
| Renderer, resolution, scaling, graphics settings | Pending; record separate D3D9 and OpenGL sessions |
| Enabled addons and optional patches | Pending; clean baseline first, modified-client compatibility separately |

## Preparation

1. Download a passing Windows x64 baseline artifact and verify its three binary
   hashes against `manifest.json`. Record the run/commit, configure/build logs and
   compiler/runner metadata. All compilation remains in GitHub Actions.
2. Prepare a separate writable staging directory with verified stock 12340 data.
   Do not alter `/mnt/games/ChromieCraft`, copy its proxy DLLs/patch-menu tools into
   the baseline, or silently merge its case-colliding patch names on Windows.
3. Put `WowClient64.exe`, `Wow64.exe` and `XvidDecoder.dll` in staging and supply
   `Data/<locale>` using the documented Windows layout. Configure only the intended
   test endpoint and session settings in staging; preserve signature checks.
4. Use the crash-logging client first. Save initial settings/logs, then make separate
   labeled D3D9 and OpenGL sessions using the supported graphics configuration
   (`gxApi`/client arguments). Confirm the selected renderer from startup output.

## Scenario results

Use `pass`, `fail`, or `not run`, with an observation/log reference for every row.
An existing failure remains a failure and should be compared with the original
client on the same clean data/server setup where possible.

| Scenario | D3D9 | OpenGL | Observation/evidence |
| --- | --- | --- | --- |
| Startup/data verification/login UI | Not run | Not run | |
| Authentication and character selection | Not run | Not run | |
| World entry and movement/camera/input | Not run | Not run | |
| Combat/spell effects, inventory and chat | Not run | Not run | |
| Zone transition/teleport and resource loading | Not run | Not run | |
| Outdoor terrain, indoor WMO and skinned characters | Not run | Not run | |
| Water, shadows, particles and UI/text | Not run | Not run | |
| Positional sound, music, volume and movie playback/skip | Not run | Not run | |
| Focus, fullscreen/resize, mouse capture and text input | Not run | Not run | |
| Settings save/reload and addon API compatibility | Not run | Not run | |
| Sustained world session and Warden outcome | Not run | Not run | |
| Disconnect/reconnect, logout and clean shutdown | Not run | Not run | |

Run the world session for at least 15 minutes to cover the desired deployment's
30-second Warden holdoff and 600-second response-delay window; record actual check
timing/outcomes. This is an initial observation window, not a guarantee that every
possible check or gameplay path has been exercised. Do not disable Warden or client
verification to qualify the baseline.

For each renderer, capture repeatable views of outdoor terrain, indoor world
objects, a skinned character, spell effects, water and UI. Record map/location,
camera, settings and frame-time observations. Do not compare different GPUs or
settings as if they were an isolated renderer-performance comparison.

## Result

- Baseline established: **pending**.
- Pre-existing failures and reproduction details: **pending**.
- Evidence locations and retention: **pending**.
- Remaining validation gaps and next action: **pending**.
