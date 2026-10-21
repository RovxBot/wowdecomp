# Native image lookup and atlas eligibility

This follows [authored mips/readiness](IMAGE_MIPS.md) and the
[Frame font factories](FRAME_FONTS.md). It adapts product 12340
`TextureCreate`, `PumpBlpTextureAsync`, `CTextureAtlas::Get/Free` and the original
atlas coordinate API at the native Services boundary. No donor code, new
dependency, game data or new binary-verification claim is introduced. The original
Windows implementation remains separate.

`TextureCreate` now interprets Frame's `0x17`: explicit sampler, disk eligibility,
atlas eligibility and UI priority. An optional constructor argument mounts one
read-only loose-image directory. Disk-eligible requests check that root before
authenticated packages; without a mounted root they use packages. Package-only
requests never check disk. Only an absent loose path allows package lookup;
case collisions, escaping symlinks, nonregular files, failed reads and invalid
BLPs retain failure status. Root routing assumes stable directory topology, as
in [native host files](../m2/FILESYSTEM.md). No process working directory or game
installation is discovered or changed. This is an explicit loose image source,
not a new fallback in the authenticated package reader or interface loader.

Disk policy and atlas representation participate in cache identity. Existing
handles retain their representation when atlas creation is disabled. UI priority
continues to promote identical queued requests without changing their handles.
Reads/decode execute on the retained worker; source selection never publishes
native tickets as `CAsyncObject`. TGA/alternate-format search, basename aliases,
texture substitutions and remote streaming remain unavailable and fail visibly.

Atlas construction is disabled initially, matching the original global. With
`TextureEnableAtlas(1)`, BLP header flag `0x10`, square dimensions at most 64 and
an explicit supported sampler admit an image. The native sheet supports nearest
and linear clamped sampling at mip zero. Repeat or mip sampling retains a
standalone texture, as do unmarked, large or nonsquare images. This bounded
eligibility does not claim the complete original atlas/mip source callback ABI.
Marked images carry the original caller's responsibility for safe edge texels.

Each sheet owns 64 cells in the original 8 by 8 grid. `CTexture::m_atlas` and
`m_atlasBlockIndex` describe real shared GPU storage; `TextureGetGxTex` returns
the sheet while `TextureGetDimensions` returns the member size. The original
offset is `(column/8, row/8)` and scale is `1/8`. The sheet callback supplies a
retained whole-image origin to the qualified Gx upload service; it does not
pretend to implement legacy sampler bit 15. Sheets share the existing 256 MiB
pixel budget. Final member close retires the sheet GPU bindings before pixels;
other closes clear and recycle their slots. Close all handles before the service,
then device/context/queue. Public legacy metadata is borrowed owner-thread state.

Requests without explicit sampler bit 0 use the original default
`GxTex_LinearMipNearest`, configurable with `TextureSetFilterMode`. Original
small single-level BLPs (width below 256) select nearest under this default-policy
path. Explicit mip requests retain the prior complete authored-chain requirement;
there is no generated chain or silent downgrade for them. Anisotropy remains
unsupported. Default selection participates in cache identity.

`NativeImageCheck` retains three complete cycles and now exercises actual loose
files, package-only/eligible cache separation, absent versus invalid source
selection, case ambiguity, symlink escape, twelve atlas slots across two rows,
sharing, sibling survival, changed-pixel slot reuse, nearest/linear samples,
ineligible repeat/mip sources, atlas enable state and default filtering. Authored
files live in a temporary root outside game data. Optional original-data mode
requests the unchanged loading-bar image with `0x17`, reports actual atlas
membership and retains the original mip-chain regression. This does not create
Frame objects or render original GlueXML/login.

All client and dependency compilation runs in
[M3 GL Actions](../../.github/workflows/m3-gl.yml); full Windows x64 compilation
remains the regression reference. Qualification is pending hosted checks and
downloaded exact-source release/strict ASan/UBSan/leak runtime evidence.

Run downloaded, manifest-verified packages without compiling locally:

```sh
python3 Tools/Porting/run_native_gl.py --artifact /tmp/verified-image-package \
  --images --data /mnt/games/3.3.5aClean/Data \
  --settings /tmp/image-source-settings --cache /tmp/image-source-cache \
  --output /tmp/image-source-runtime.json --snapshot /tmp/image-source.ppm
```

Use `--sanitizers` with the sanitizer package and separate writable roots.
Actual regions/font strings/root batching, SDL size invalidation, Frame texture
image modes/factories, full signed ordered GlueXML execution and its second
post-creation digest comparison remain open, followed by login rendering,
transport and observed gameplay. No physical-input, Windows runtime, macOS
graphics or server acceptance is added. M3 is incomplete; signatures, server
revision discrepancies and interactive user sign-in remain intact.
