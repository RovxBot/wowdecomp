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

[PR #80](https://github.com/RovxBot/wowdecomp/pull/80) is qualified at
`5b0c26a9a8f5da7044bbf9352235bb7b411d3a76`; the [durable record](backdrop-validation.json)
retains exact hosted/package/runtime identities and prior failures. Final-source
Linux GL release/strict ASan/UBSan/leak configurations passed 38 CTest checks each;
four native asset configurations passed eight checks each, including actual ARM64
macOS execution. Full signature-enabled Windows x64 compiled at
`05aad12dd224256928192968233863501e078645`. Later changes are native-only guards,
Portable decoder code and fixture/audit inputs; Windows-selected generator/Frame
tokens remain unchanged. These are separate compilation identities. Seven archive
digests/all 67 packaged executable hashes/types/notices verified.

Downloaded RX 7900 XTX / Mesa 26.2.5 / SDL X11 release and strict sanitizer packages
passed twelve three-cycle authored/original-data executions, with empty stderr
and zero workers. All 21 capture pairs and twelve preceding CheckButton capture
comparisons match. The inspected original-field backdrop has PPM SHA-256
`3d604e3006932fcf47634ddb594e6e72b2a57785196c046c4e1950adc4a16396`.
Original signaturefile authentication observed 28,948 records. Capture placement
is authored; no user input or credentials were supplied. Local source/launcher
checks: 57 passed, one unavailable-CMake skip; Windows token and whitespace checks
passed. No client/dependency compiled locally.

EditBoxes, remaining widgets, complete signed ordered GlueXML and its full second
digest gate, actual login, transport/authentication and gameplay remain open. Clean data stays read-only, signatures and server revision
discrepancies remain intact, and user sign-in stays interactive. M3 is incomplete.


Reproduce after downloading and verifying a passing Actions GL package:

```sh
python3 Tools/Porting/run_native_gl.py --artifact /tmp/verified-backdrops \
  --frame-hierarchy --data /mnt/games/3.3.5aClean/Data \
  --settings /tmp/backdrop-settings --cache /tmp/backdrop-cache \
  --output /tmp/backdrop-runtime.json --snapshot /tmp/backdrop.ppm
```

The wrapper selects strict detectors automatically for the `linux-gl-asan`
package. Use separate user roots/capture names per mode. The additional original
backdrop capture is `<snapshot>.backdrop.ppm`; the final main snapshot remains the
historical authored hierarchy text. Fixture input is injected; no physical-input
or interactive sign-in acceptance is inferred.
