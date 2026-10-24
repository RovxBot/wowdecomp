# Original tiled Frame backdrops

This package selects original 12340 `CBackdropGenerator` through base Frame XML
and Lua backdrop methods. The nine background/side/corner textures use the
qualified aligned region pool, existing file image Services and retained Gx
batches. No donor code, game data or dependency is added. Windows-selected bodies
retain their original tokens.

Replacement belongs to the Frame owner outside hierarchy traversal and batch
collection. The caller transfers the generator only on successful `SetBackdrop`.
New regions are built first; failed admission returns every temporary slot and
leaves the preceding backdrop owned. The original Frame region destructor closes
textures; the generator then closes without touching retired region pointers.
Resize regenerates original side/background UVs. Blend state is initialized before
XML use. Sizes, insets, image paths and UV quotients are checked before output.
Native background sampling repeats in both axes, matching its generated U/V
coordinates; the historical Windows background sampler keeps its literal V-clamp
argument. Corners retain clamped sampling and original inset/edge coordinates.
Repeat textures stay standalone under the existing atlas policy.

Native Lua uses raw table field access to prevent table hooks reentering partial
replacement/output state. Getters retain optional result-table reuse. Template
nodes and interned image names survive source-tree close through the existing XML
owner; close Frames before Scripts/images/renderer/Fonts/layout/XML/VM/device.
General texture tiling mutators, animations and other widgets remain unavailable.

The hierarchy fixture exercises XML/templates, original dimension/inset helpers,
Lua round trips/colors, repeated pixels in both axes, resize/restore, allocation
rollback, replacement/removal, stale tables, owner/collection guards and pooled
teardown. Original-data mode selects the unchanged backdrop node and dimensions
from an AccountLogin EditBox, renders its original tooltip assets, compares exact
repeats and checks InterfaceKey before/after the subset. The surrounding frame
and placement are authored. This is not an EditBox or full AccountLogin execution.

Original-data inspection exposed a further original thin DXT tail: the 128x16
Glue tooltip border stores one block for its unrequested 16x2 level. Explicit
base-only reads admit only that additional bounded shape with power-of-two base
and mip flag; all spans remain checked. Requested mip-chain reads still reject
missing blocks. DXT1/3/5 authored fixtures compare complete base pixels and reject
malformed/truncated/nonthin/unflagged storage in both axis orientations. No tail
pixels are decoded, fabricated or uploaded. The earlier native rejection and
release-only GDB header diagnostic remain evidence, separate from qualification.

All compilation runs in GitHub Actions. Final hosted and real-GPU qualification is
pending. EditBoxes, remaining widgets, complete signed ordered GlueXML and its
full second digest gate, actual login, transport/authentication and gameplay
remain open. Clean data stays read-only, signatures and server revision
discrepancies remain intact, and user sign-in stays interactive. M3 is incomplete.
