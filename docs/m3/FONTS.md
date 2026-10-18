# Native original font face and glyph preview

This M3 package loads the original `Fonts/FRIZQT__.TTF` through authenticated
12340 packages and renders a small text sample through the existing original UI
shaders, retained ARGB4444 textures and PCT buffers. It is a font/resource preview,
not GlueXML/login, full string layout or gameplay. M3 remains open.

## Source and ownership audit

Source reference: merged main `365016e226547f7f3867f419988ccdf6b3182f25`.
The bundled FreeType sources are selected explicitly in `WowNativeFonts`.
No system font library, new download, donor body, original binary address or
VERIFY claim is imported. FreeType remains credited to the FreeType Project;
the artifact carries its [existing license](../../ThirdParty/freetype/docs/FTL.txt).
The original Windows `freetype` and Gxu source selections remain separate.

| Original source | Native adaptation and limits |
| --- | --- |
| `GxuFontFace::FontFaceGetHandle`, `FACEDATA` | Retained memory bytes and owner-thread FreeType face/library destruction. Application bytes come from `Packages::Open12340` and its original-key/registered-MD5 policy. Unicode scalable faces are required. Native `FontFace` is not a legacy HFACE, handle hash/cache or CGxFont layout. |
| `GxuFontMain`, `FreeTypeAlloc/Free/Realloc` | Select bundled TrueType/SFNT and grayscale/monochrome raster modules with Storm-backed allocator callbacks, checking signed allocation sizes. Face teardown precedes library and source-byte destruction. All access belongs to one owner thread, separate from the retained file worker and GPU/context ownership. |
| `IGxuFontGlyph::FREETYPE_RenderGlyph`, `CGxFont::GetGlyphData/UpdateDimensions` | Preserve pedantic/unhinted flags, normal/monochrome render selection, 1..32 pixel range and integral 26.6 advance +1. Own bounded top-down coverage, validate pitch/bitmap modes/metrics and expand monochrome bits. Empty whitespace coverage retains its advance; missing glyphs fail explicitly. Native bounds and safe pitch handling are adaptations, not binary revalidation of the original glyph wrapper. |
| `CGxFont::GetGlyphAdvance` proportional spacing | Preserve negative-only unscaled kerning, x-ppem/units-per-em conversion and ceil of the resulting step. This is a focused pair service, not the original cache, rich-text markup, wrapping, line anchoring or bidi/shaping behavior. |
| `TEXTURECACHE::PasteGlyphNonOutlinedAA`, `BATCHEDRENDERFONTDESC` | Preserve the original grayscale-to-ARGB4444 nibble encoding, atlas row convention and 24-byte PCT layout. The preview packs its fixed sample and submits buffered triangles. It does not implement the original row eviction, outline/shadow/gradient/stereo/string cache or native legacy dispatch. |

Native source faces are bounded to 32 MiB. Raster bitmaps/row spans are bounded,
and bearings/advances outside the bring-up range fail. Each returned glyph owns
its coverage after FreeType reuses its slot or the face is destroyed. No pointer
into the file worker or FreeType slot survives into GPU upload.

The native module configuration supports TrueType outlines with the original
unhinted load flags. CFF/Type1 and auto-hinting are outside this service and fail
when no driver can decode the face. Native-only typed adapters correct the old
FreeType module, driver, raster and outline callback signatures; signed coordinate
scaling uses multiplication instead of left-shifting negative values. All changes
are guarded by `WOWDECOMP_PORTABLE_RUNTIME`; the Windows module list and original
callback/scaling expressions remain selected in the full client. This qualification
covers the exposed face/raster/kerning service, not every bundled FreeType API.

Original-data reads stay on the retained worker; the owner callback joins/drains
before creating the face and GPU resources on the main thread. The probe reads
an original font member, reports its registered-MD5 status, verifies a rendered
glyph through the original UI shader in an FBO, then displays text. It does not
substitute synthetic fonts or disable checks when original data fails.

## Validation

All C/C++ and dependency compilation runs in
[M3 Actions](../../.github/workflows/m3-gl.yml), through `linux-gl` and
`linux-gl-asan`. Graph policy allowlists the native face source and eight
existing bundled FreeType C entry units, plus `NativeFontsCheck`.
`native-original-font-raster` exercises retained memory, grayscale/monochrome
coverage, whitespace, actual negative AV kerning, malformed input, Unicode/size
bounds, owner-thread rejection and repeated teardown using a 1 KiB authored
geometric TrueType fixture. Its [license/generator](../../Tools/NativeGx/fonts/LICENSE.txt)
is separate from game fonts; ordinary CI contains no retail font data.

After downloading a passing artifact, the existing hash-checking
[wrapper](../../Tools/Porting/run_native_gl.py) runs original-data mode and all
requested resource fixtures. Use the [texture command](TEXTURES.md#validation)
with the font artifact path. Its output must report original font raster/atlas/
UI-shader pixels and zero workers; release capture inspection and sanitizer
cycles remain separate from hosted compilation. No new physical input, original
login UI, Windows gameplay, macOS graphics or server session follows from these
checks. The user will sign in interactively once native authentication is ready.
The external [server revision discrepancy](SERVER_IDENTITY.md) is preserved.

Hosted and real-GPU qualification is pending. All sanitizers remain enabled;
any bundled dependency failure must be retained and corrected, not suppressed.
The first implementation `9712f9dbdbf53ccd9bcfa789818dc01213ffc248`
passed release raster/GPU checks but failed the sanitizer fixture at the old
`ft_autohinter_init` constructor callback (`ftobjs.c:2308`), in
[run 39110055917](https://github.com/RovxBot/wowdecomp/actions/runs/39110055917).
That failure motivated the explicit native module scope and callback audit above;
it is not a clean sanitizer result.
