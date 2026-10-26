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
native tickets as `CAsyncObject`. The PR #84 startup continuation adds the original
BLP-first basename lookup for `.tga` requests unless exact-suffix bit 3 is set.
The `.tga` and `.blp` spellings share the actual BLP cache entry. This does not
decode TGA or provide TGA fallback, broader format substitutions or remote
streaming; those remain unavailable and fail visibly. See the
[integration evidence](integrated-model-render-validation.json).

Atlas construction is disabled initially, matching the original global. With
`TextureEnableAtlas(1)`, BLP header flag `0x10`, square dimensions at most 64 and
a supported sampler admit an image. The native sheet supports nearest
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
other closes clear and recycle their slots. Each cell update finishes immediately,
as in the original atlas: legacy Gx dirty rectangles replace rather than accumulate.
Close all handles before the service,
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
remains the regression reference. The qualification below records the selected
implementation; the original Windows source and prior reports remain intact.

## Qualification

[PR #43](https://github.com/RovxBot/wowdecomp/pull/43) is qualified at
`e308d2e7eedc1e8f8cba7cfc8cec885e8fe5bfa6`, merged as
`dbb4a00d8c85b74ad11f34a8a5f68aa91692aa1e`. The
[durable record](image-source-validation.json) retains thirteen passing PR
workflows, 31 CTest checks per Linux release/strict ASan/UBSan/leak configuration
and full Windows x64 compilation with signatures enabled. Hosted checkout
`e520213854a27c5fb2c694e3effbb0b9e3443821` has the same tree as the selected
implementation and merge. Three archive digests and all 45 packaged binary
hashes/types were verified. No client/dependency compiled locally.

Downloaded release and strict sanitizer binaries each passed three authored image,
three original-data image and three original GlueFonts cycles on RX 7900 XTX /
Mesa 26.2.5 / SDL X11. All six executions exited 0 with empty stderr and zero
workers at shutdown. Original-key signaturefile authentication observed 28,948
records; selective registered-member MD5 checks remained enabled. The font
regression authenticated the original interface signature and constructed eighteen
GlueFonts definitions each cycle. Release/sanitizer loading-bar captures match
each other and the previous qualified hash; the release capture was inspected.
The original BLP lacks the atlas flag and remains standalone. Shared-atlas pixels
and loose overrides are authored evidence, not original marked-atlas or login
rendering. The unchanged Elwynn authored-mip regression remains separate from
world rendering.

The first hosted release compiled and passed 30/31 checks, then failed a fixture
assertion because a package-only read was queued before the disk-only read-count
assertion. Construction order was corrected; implementation and detectors were
unchanged. That failure remains in the record; its superseded sanitizer job was
cancelled and is not qualified. Local Python source/launcher checks: 48 passed,
one CMake availability skip.

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

## Next original consumers

1. Connect `CScriptRegion`/`CSimpleRegion` with actual parent, layer and animation
   lists, then `CSimpleFontString` bodies from `CSimpleRender.cpp`. Initialize
   native fields before preserving bit masks, including `m_maxLines`. Preserve
   original font/text-block close order and the qualified font/name/layout owners.
2. Connect actual `CSimpleTexture` factories/image modes. Its constructor preserves
   uninitialized `m_textureFlags` bits in the original path; initialize native
   storage before masking. Bound image-mode indices and qualify the original UI
   and Desaturate shaders, handle replacement, wait-2 diagnostics and atlas UVs.
   Services accepting `0x17` alone does not establish these consumers.
3. Connect actual `CSimpleTop`/Frame consumers to the qualified
   [retained batcher](FRAME_RENDER.md), with deferred destruction and real SDL
   size events. Full-width sorting and low-level resource retention are qualified;
   actual region/list lifetimes and native Frame-object queue entry points remain
   open. Keep queued regions alive through batch consumption.
   Execute both `GxuFontWindowSizeChanged` and each original frame/font-string
   screen-size callback, then close consumers before layout/font/device owners.
4. Connect actual FrameXML factories, retained templates and ordered Lua traversal.
   Preserve the second complete content digest comparison in `CGlueMgr::Resume`
   before FRAMES_LOADED, then execute/render original GlueXML/login before the
   user's interactive sign-in. The direct GlueFonts regression is a subset.
