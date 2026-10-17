# M0 Windows runtime session record

Status: operator-reported native Windows results recorded on 2026-10-17.
D3D9 passed the reported scenarios; OpenGL has repeatable scene-rendering and busy
cursor failures. Hardware rendering evidence and server/data identity remain
partial as listed below. These observations do not validate a native Linux/macOS
client. Original scenario observations are preserved below.

## Session identity

| Field | Observed value |
| --- | --- |
| Date/time and operator | Oct 17 2026, sam|
| GitHub Actions run and source commit | Run [38845726826](https://github.com/RovxBot/wowdecomp/actions/runs/38845726826), `5fece2ca698c53680174a4b8c20948b5c484ab21`; identified by the supplied artifact ZIP digest |
| Artifact name and SHA-256 manifest | `windows-x64-baseline-5fece2ca698c53680174a4b8c20948b5c484ab21`; artifact ZIP SHA-256 `012c8f5f216cdebc8ef1b391ded3aeaa12bdb135cc674d7d8ffb2184a14666d5`. Per-binary verification was not separately recorded by the operator. |
| Windows edition/build, CPU, RAM | win11 Enterprise - 25H2, 16GB DDR3, i7-8750H, Nvidia RTX 2060  |
| GPU and driver version | NVIDIA RTX 2060; GeForce Game Ready driver 572.42 (operator follow-up) |
| VC++ runtime/toolchain dependencies | Pending |
| Data source, build, locale, archive hashes/provenance | Operator reports clean unmodified 3.3.5a data; locale, archive hashes and provenance not recorded |
| Live GrimGuzzler core revision/image digest | Pending; deployment-source tag is recorded in BASELINE.md |
| Auth and realm/world endpoints | Pending verification; keep credentials external |
| Server Warden settings and observed checks/timeouts | Pending |
| Renderer, resolution, scaling, graphics settings | recorded at 1920x1080, record separate D3D9 and OpenGL sessions |
| Enabled addons and optional patches | no addons or patches |

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
| Startup/data verification/login UI | pass | fail | Opengl login backgrounds are warped and boxed no crashing and the UI buttons and text boxes are fine and working.|
| Authentication and character selection | pass | pass | logged in with opengl but game is buggy and looks like it is trying to run in 3d for VR almost.|
| World entry and movement/camera/input | pass | pass | but again opengl is buggy |
| Combat/spell effects, inventory and chat | pass| pass | |
| Zone transition/teleport and resource loading | pass | pass | |
| Outdoor terrain, indoor WMO and skinned characters | pass | fail | |
| Water, shadows, particles and UI/text | pass | fail | UI on opengl is perfect. rendering is bugged out and warped |
| Positional sound, music, volume and movie playback/skip | pass| pass | |
| Focus, fullscreen/resize, mouse capture and text input | pass | fail | opengl mouse capture is fine, but constant loading circle appears |
| Settings save/reload and addon API compatibility | pass| pass | |
| Sustained world session and Warden outcome | pass | pass | |
| Disconnect/reconnect, logout and clean shutdown | pass | pass | |

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

- Native Windows D3D9: operator reports all listed scenarios passed.
- Native Windows OpenGL: baseline failure reproduced in both M0 and M1. Login
  backgrounds and world scenes are warped/boxed; UI controls/text remain usable;
  focus/resize scenario reports a persistent busy cursor. No M1-specific change
  in these outcomes was reported.
- Baseline status: **partial**. The results establish reported behavior, including
  a pre-existing OpenGL failure; they do not establish a passing OpenGL baseline.
- Artifact identity is resolved from the supplied ZIP digest. The runtime records
  do not cover the later upstream cursor fix at `4119c4b` or subsequent M1 work.
- Remaining evidence: locale/archive hashes and clean-data
  provenance, actual live server revision/image/endpoints/settings, session
  duration/Warden check logs, scene/settings captures and timing/log locations. The operator confirms no logs
  or screenshots were collected.
  The Warden scenario's reported pass means no observed failure was reported;
  actual server checks and compatibility remain unverified without those records.
- No game data or cluster/server policy was changed by this review.
