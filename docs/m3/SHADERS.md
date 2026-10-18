# Native 12340 shader service

This M3 slice adapts the first shared Gx resource service to the native context:
bounded `0x10003` containers and context-owned ARB programs. The full native
client remains gated. Original fonts, GlueXML/FrameXML, addon verification, the
complete device and world renderer, networking and gameplay remain open.

## Source and caller audit

The reference is this checkout's 12340 implementation at
`0786df65a30135c833d573bfda7836cf5cff8d70` (the parent main). No 15595 or native-fork body is imported.
The new files carry no original-address or binary-verification annotations.

| Existing boundary | Native adaptation and remaining work |
| --- | --- |
| `CGxDevice::IShaderLoad` and `CGxShader::Load` | Decode HSXG magic, version, opaque metadata, caller-counted permutations, code extents and four-byte padding with explicit little-endian reads. Preserve the opaque header word without calling it a count. No native struct overlay. |
| `CGxDeviceOpenGl::IVertexShaderPatch` / `IPixelShaderPatch` | Convert the bounded `program.local` namespace token to `program.env`, preserving source length and comments. This arbvp1/arbfp1 service retains the original vertex range; 192/223-register model expansion and cg12/NV/D3D dialects need separate adaptation. |
| `IShaderCreateVertex` / `IShaderCreatePixel` / `IShaderConstantsSend` | Context-owned `GlProgram` uploads ASCII excluding its required final NUL, checks error position and native limits, restores the prior binding on rejected upload, and sends environment float4 constants. All GPU operations and destruction require the current owner context on the main thread. |
| `GxuFontInitialize` / `CGxStringBatch::RenderBatch` | Their original UI paths request two vertex permutations and one fragment permutation, setting transposed view-projection at vertex constants 0–3. The native probe checks those containers, uploads both vertex variants and renders permutation zero with an explicit identity matrix and Gx attribute indices. Upload of permutation one does not qualify stereo rendering or font rasterization. |
| `IDeviceSetRenderTargetFBO` / `DeviceSetRenderTarget` | Existing legacy code changes color/depth attachments, viewport and projection together and can create an implicit depth renderbuffer. The probe's existing fixed FBO is retained; a complete native target owner still needs attachment replacement, default-drawable restoration, depth-only/cube-plane handling, resize and feedback/lifetime checks. No legacy device state is reinterpreted here. |
| `CM2Shared` / `CM2Model` asynchronous callers | Model reads retain near/aligned buffers, cancel/free callbacks and queue-lock/list interactions. Their `CAsyncObject` cannot be replaced by a native ticket. This shader read instead uses a retained native operation, joins the worker and drains its one owner callback before uploading. It does not adapt model streaming or animation ownership. |

`ShaderPackage` owns decoded bytes. Files are limited to 64 MiB, 65,536
permutations and 1 MiB per program. This first service requires the caller's
count to consume the whole container; unrecognized trailing data fails visibly.
Empty code records can decode but cannot create a GPU program. Unsupported
versions, dialect/stage mismatches, missing terminators and embedded NULs fail.

`GlProgram` must die before its context; copying is disabled. Failed construction
deletes the allocated name. Binding and constant delivery reject foreign threads
before touching GL. Constants beyond the device's environment limit fail.
This native resource does not expose or substitute a `CGxShader*` or its legacy
hash/refcount/cache contract. The Windows implementation remains separately
selected and unchanged.

## Validation and operator use

Compilation and C++ fixtures run only in
[M3 Actions](../../.github/workflows/m3-gl.yml), for `linux-gl` and
`linux-gl-asan`. The allowlist rejects legacy device objects and signature-bypass
definitions. `ShaderPackageCheck` exercises malformed/truncated/version/padding
and adaptation cases. `NativeGlCheck` retains five texture/depth/blend/FBO sample
pixels, DXT1 readback, repeated teardown and dummy-driver rollback, and adds
program upload rollback, constant bounds and thread-affinity checks.
Hosted Xvfb/Mesa output is software rendering, separate from real GPU evidence.

Original-data mode uses `Packages::Open12340`, the original RSA key and existing
registered MD5 policy. It reads `Shaders/Vertex/arbvp1/UI.bls`,
`Shaders/Pixel/arbfp1/UI.bls` and the loading-bar BLP from the unchanged external
MPQs. It reports whether each shader has a registered MD5. Missing or rejected
shaders fail instead of falling back to the synthetic program. Before displaying
the original texture, it checks the same five synthetic FBO pixels using the
original shaders. This is original shader execution, not the original login UI.

After downloading a passing artifact and retaining its manifest, use the
hash-checking wrapper from the repository:

```sh
python3 Tools/Porting/run_native_gl.py \
  --artifact /tmp/wowdecomp-m3-shaders-release \
  --data /mnt/games/3.3.5aClean/Data \
  --settings /tmp/wowdecomp-m3-shaders-settings \
  --cache /tmp/wowdecomp-m3-shaders-cache \
  --frames 3600 --timeout 120 \
  --snapshot /tmp/wowdecomp-m3-shaders.ppm \
  --output /tmp/wowdecomp-m3-shaders-operator.json
```

Use the absolute wrapper path when launching from another directory. Keep every
output outside the installation. For sanitizers use the sanitizer artifact and
`--sanitizers --cycles 3 --frames 30`, with a separate output record. Do not
suppress leak or external-library failures. No build command runs locally.

Operator checks on the new artifact: the blue loading-bar rectangle should remain
visible through resize, minimize/restore, focus changes and F11; Escape/window
close should exit normally. Record the report, GPU/driver/session and any visual
failure. The original-data shader-probe acceptance below is separate from the
older context-probe observations. Fixed Windows scene captures, original native
login/UI, authenticated server login, character selection/world entry and Warden
observations remain separate checks.

[Durable validation](shader-validation.json) records selected head
`543243c676f16b1a2c32af38d8b0910c73eaadbd` and its merge in
[PR #17](https://github.com/RovxBot/wowdecomp/pull/17). All eight PR workflows
passed, including full Windows x64 compilation with signatures enabled. Both
Linux GL configurations passed 15 CTest checks. Downloaded binaries and notices
matched their manifests; Windows binaries also matched PE AMD64 identities.

On RX 7900 XTX / Mesa 26.2.4 / SDL X11 on Xwayland, exact-source release and three
ASan/UBSan/leak-enabled original-data cycles passed, with exit 0, empty stderr and
zero workers. Both original UI vertex permutations uploaded. Permutation zero
and the original fragment program passed all five FBO samples, and the captured
original loading-bar output was inspected. No input was injected. Both shader
members lack registered MD5 entries; this is the unchanged selective policy,
not an MD5 pass for those files. The original-key signaturefile authenticated
28,948 records. Captures/game data remain external.

On 2026-10-18 the operator reported all requested window checks passed: resize,
minimize/restore, focus changes, F11 and window close, with no crash or corruption.
The saved original-data release report matches selected head `543243c` and its
hosted binary hash; it records exit 0, no timeout, empty stderr and zero workers.
The [operator acceptance](shader-validation.json) preserves the statement and raw
report separately from automatic GPU/sanitizer runs. There is no per-event
telemetry or new operator capture. The earlier M3 context-probe acceptance retains
its own source identity. New Windows runtime, original fonts/login UI and observed
native gameplay remain unavailable.
No local client/dependency compilation, game-data mutation or server-policy
change occurred.
