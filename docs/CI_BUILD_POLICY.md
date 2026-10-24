# Hosted build scheduling

Automatic builds run for matching pull-request changes and matching pushes to
`main`. Feature branches do not also build on `push`: their PR event provides the
review build. `workflow_dispatch` remains available for pre-PR experiments or
manual qualification. Keep all client/dependency compilation in GitHub Actions.

The trigger-only change removes the duplicate feature-branch push run for each
matching open PR update. Every existing path filter, job, platform matrix, strict
sanitizer setting, artifact identity/retention rule, permission and concurrency
setting stays unchanged. Main regression builds still run after implementation
merges. Existing per-ref concurrency continues to cancel superseded PR runs.

Validate trigger-only edits by parsing all workflows and comparing their semantic
objects: only `on.push.branches` changes from `main, native-port/**` to `main`.
Use a hosted M0 source audit plus local source/launcher checks. Recompiling the
unchanged client for this scheduling-only PR adds no implementation evidence;
its commits/merge use `[skip ci]` to avoid starting the unchanged build matrices.
Future implementation PRs retain normal automatic validation.

For manual implementation qualification, first run the smallest affected suite.
A layout preset already builds/tests native layout, XML, FrameScript, Lua,
async/files/assets/core prerequisites on its four native configurations. Linux GL
presets add window/Gx/Gxu/Frame rendering and strict sanitizer coverage. Reuse those
contained checks where applicable; do not dispatch every lower-level workflow
again for the same source without a distinct configuration or unresolved concern.
Retain full Windows client compilation for legacy source changes and actual ARM64
execution where shared native source changed. Record exactly which configurations
ran; subset success is not a full-workflow or full-game qualification claim.

For a correction confined to a native fixture, rerun its affected preset rather
than rebuilding unchanged production targets. Suppress the overbroad automatic
PR matrices for that correction with `[skip ci]`, dispatch the affected workflow
on the exact new head, and retain the earlier passing workflow identities. Verify
and record unchanged production-source/build-input trees before reusing their
results. Report the earlier and final checkout identities separately; never label
an earlier workflow as a final-head run. Changes to production/build inputs require
fresh checks of the affected configurations.
