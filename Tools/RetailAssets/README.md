# External Forever assets

Asset preparation is tracked in [#221](https://github.com/RovxBot/wowdecomp/issues/221).
This optional Linux x86-64 tooling reads an installed Forever client offline and
stages the bounded Alpha 0.2.0 UI/font inputs outside the product checkout. It is
not a client dependency or a CASC runtime mount. Keep the installation read-only.
Game assets never enter Git or ordinary Actions artifacts.

The `Offline retail asset tooling` workflow compiles the pinned CascLib
reader without game data. Download its artifact, check the Actions ZIP digest,
and retain all files, including source and third-party notices, together.
The source archive and commit are locked in `reader.json`; client/dependency
compilation stays in Actions. CascLib is used only offline, with downloads off
and strict read checks on. Missing encryption keys are errors. Two upstream
existence probes are adapted to open files read-only; their adapted hash is in
the artifact manifest. The reader never uses `CASC_OVERCOME_ENCRYPTED`.

## Stage and verify

Use the exact Forever version and external Gethe export pinned by
`Tools/LinuxClient/RetailUI/references.json`. Staging checks the entire declared
graph, copies its selected files/provider candidates and evidence documents, and
extracts matching installed UI plus roman font dependencies from CASC. It retains
both UI byte sequences: installed files must match the export exactly or differ
only in CRLF line endings. No file is rewritten to manufacture a match. This is
an enUS bootstrap bundle; it does not claim other locale or complete dynamic UI
dependency coverage.

Run locally with Bubblewrap, keeping the installation and code read-only and
networking disabled. Only the external output parent is writable. Substitute
absolute paths and the verified library SHA256 from the Actions artifact:

```sh
bwrap --ro-bind / / --bind "$ASSET_ROOT" "$ASSET_ROOT" --unshare-net -- \
  python3 -B Tools/RetailAssets/bundle.py stage \
    --install '/mnt/games/World of Warcraft' --ui-root "$UI_EXPORT" \
    --reader "$READER/libcasc.so.1.0.0" --reader-sha256 "$READER_SHA256" \
    --output "$ASSET_ROOT/forever-1.60.1.70862-enUS"
python3 -B Tools/RetailAssets/bundle.py verify \
  --bundle "$BUNDLE" --manifest-sha256 "$BUNDLE_MANIFEST_SHA256"
```

Record the printed manifest hash separately. Every entry records SHA256, size
and origin; CASC entries also retain content/encoding keys, FileDataID and locale
flags. Source identity includes the build/CDN configuration hashes, `.build.info`
hash and before/after Data file size/mtime inventory fingerprint. The inventory
does not rehash the entire installation. A missing/unavailable/encrypted asset,
changed source, unsafe path or mismatching reference aborts staging without a
usable manifest. Do not update the source installation to force a match.

## Alpha 0.2.0 private release input gate

Compile the client in Actions without game assets. After downloading and checking
the client archive/AppImage against its hosted checksum, prepare its private
release inputs locally:

```sh
python3 -B Tools/RetailAssets/bundle.py prepare-release \
  --bundle "$BUNDLE" --manifest-sha256 "$BUNDLE_MANIFEST_SHA256" \
  --client "$ACTIONS_CLIENT" --client-sha256 "$CLIENT_SHA256" \
  --output "$PRIVATE_ALPHA_INPUTS"
```

The gate rejects missing/modified assets, unlisted files, symlinks, mismatching
client bytes or an existing output. It copies the client unchanged beside the
verified external bundle and records their identities in `release-inputs.json`.
Staging and private assembly refuse to run in ordinary Actions. The workflow
uses authored integrity fixtures only. Keep private outputs and all game assets
outside Git and ordinary CI; public release files remain the compiled client and
checksum. Final UI/runtime acceptance and packaging remain release requirements.
This gate prepares inputs; it does not launch the client or mount/activate modern
UI resources, alter signed archives, or certify complete Alpha 0.2.0 gameplay.

Fonts, UI exports and later textures are runtime/release inputs, not C++ compiler
inputs. Their availability does not establish native format support, rendering,
secure control construction or gameplay. Those requirements remain #215/#220;
model/character assets remain #214.
