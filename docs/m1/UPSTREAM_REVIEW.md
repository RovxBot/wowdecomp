# Upstream fixes retained during M1 integration

Reviewed on 2026-10-17. Product main and 3.3.5a upstream both identified
`4119c4b590ddd6934045b00f78b4b15ba39f7be2` before PR integration. Relative to the
M0 product baseline `cd29da38b9162419ee0fa29c6b16e07e10f6c4f0`, the new product
change is `5b41f72` plus its upstream merge `4119c4b` (PR #7).

- `5b41f72`: relative mouse entry now captures the current client cursor position,
  uses a saved anchor with an edge-safe margin, converts that anchor to screen /
  physical coordinates, and recenters after resize/move/reactivation. Failed
  physical-position queries no longer emit relative movement from an invalid
  point. Changes are present in both Os/Win32/Input.cpp and Os/Win64/Input.cpp.
- Reviewed the call chain: SaveMouse stores client coordinates; CenterMouse uses
  GetClientRect, ClientToScreen and the existing logical-to-physical helper. This
  corrects the old window/screen midpoint calculation and avoids the initial jump
  to the center. The one-pixel edge nudge and small-window centering are retained.
- The earlier tutorial reverse-search bounds fix from `ba9bea1`, included by
  `cd29da3`, is retained. It starts at the final valid tutorial index (59).

Both Input.cpp files and CGTutorial.cpp are compared byte-for-byte against
`4119c4b` after integration. No runtime-service change modifies them. Existing
Win32 VERIFY comments on upstream-modified input functions predate that upstream
adaptation; this review makes no new binary-verification claim for those bodies.

The user's M0/M1 runtime artifacts predate the mouse fix. Hosted compilation checks
its integration, while cursor behavior under DPI scaling, window moves and focus
changes still needs a later observed Windows session. The existing OpenGL visual
failure is recorded separately and is not attributed to this input change.

PR #1 (M0) and PR #2 (initial M1 build isolation) were refreshed with this history,
passed their Windows/source/core checks as applicable, and were merged into main
before the separate runtime-services change.
