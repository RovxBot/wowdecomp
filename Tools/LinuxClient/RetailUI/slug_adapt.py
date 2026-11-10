"""Bounded full-client Vulkan/TrueType Slug integration; source preparation only."""
import shutil


def adapt(root, edit, replace, here, added):
    for name in ['Slug.h', 'Slug.cpp']:
        target = root / 'Engine/Source/Gxu' / name
        shutil.copy2(here / 'RetailUI' / name, target)
        added.append(str(target.relative_to(root)))
    shaders = 'Engine/Source/Gx/CGxDeviceVulkan/Shaders/'
    for name in ['Slug.glsl', 'Slug.LICENSE']:
        target = root / shaders / name
        shutil.copy2(here / 'RetailUI' / name, target)
        added.append(str(target.relative_to(root)))

    def font_xml(t):
        t = '#include "Engine/Source/Gxu/Slug.h"\n#include "Engine/Source/FrameScript/RetailUI/LuaCompat.h"\n' + t
        return replace(t, '            const char* msdf = node->GetAttributeByName("msdf");', '''            if (const char* slug = node->GetAttributeByName("slug")) {
                if (RovRetailUI::Enabled() && !std::strcmp(slug, "true")) fontFlags |= RovSlug::TextFlag;
            }
            const char* msdf = node->GetAttributeByName("msdf");''')
    edit('Engine/Source/Frame/CSimpleFont.cpp', lambda t: '#include <cstring>\n' + font_xml(t))
    edit('Engine/Source/Services/TextBlock.cpp', lambda t: replace(t,
        '    if (GxuFontCreateFont(fontName, height, hashObj->m_font, gxFlags)) {',
        '    gxFlags |= fontFlags & 0x40; // Product-private Slug flag, preserved through Services.\n\n    if (GxuFontCreateFont(fontName, height, hashObj->m_font, gxFlags)) {'))
    edit('Engine/Source/Gxu/GxuFontFont.h', lambda t: replace(t,
        '    bool UsesMsdf() const;', '    bool UsesMsdf() const;\n    bool UsesSlug() const { return (m_fontFlags & 0x40) != 0; }'))

    def font(t):
        t = '#include "Engine/Source/Gxu/Slug.h"\n' + t
        t = replace(t, '    return (this->m_fontFlags & GxFontFlag_Msdf) &&',
                    '    return !UsesSlug() && (this->m_fontFlags & GxFontFlag_Msdf) &&')
        t = replace(t, '    this->m_pixelSize = pixelSize;', '    if (UsesSlug()) pixelSize = 32; // Curve coordinates use a size-independent reference em.\n    this->m_pixelSize = pixelSize;')
        t = replace(t, '    if (this->UsesMsdf()) {\n        this->m_cellHeight =',
                    '    if (UsesSlug()) {\n        this->m_cellHeight = RovSlug::Cell;\n    } else if (this->UsesMsdf()) {\n        this->m_cellHeight =')
        t = replace(t, 'bool CGxFont::GetGlyphData(GLYPHBITMAPDATA* data, uint32_t code) {', '''bool CGxFont::GetGlyphData(GLYPHBITMAPDATA* data, uint32_t code) {
    if (UsesSlug()) return RovSlug::BuildGlyph(FontFaceGetFace(m_fontFaceHandle), m_pixelSize, code, m_baseline, data);''')
        t = replace(t, '    this->m_fontFlags = fontFlags;', '''    if ((fontFlags & RovSlug::GxFlag) && g_theGxDevicePtr && g_theGxDevicePtr->m_api != GxApi_Vulkan) return 0;
    this->m_fontFlags = fontFlags;''')
        return t
    edit('Engine/Source/Gxu/GxuFontFont.cpp', font)

    def cache(t):
        t = replace(t, '    int32_t msdf = cache->m_theFace && cache->m_theFace->UsesMsdf();',
                    '    const bool slug = cache->m_theFace && cache->m_theFace->UsesSlug();\n    int32_t msdf = slug || (cache->m_theFace && cache->m_theFace->UsesMsdf());')
        t = replace(t, '                cache->PasteGlyphMsdf(glyph->m_bitmapData, dest);', '''                if (slug) {
                    const auto* source = static_cast<const uint32_t*>(glyph->m_bitmapData.m_data);
                    for (unsigned row = 0; row < 32; ++row) memcpy(dest + row * 256, source + row * 32, 32 * sizeof(uint32_t));
                } else cache->PasteGlyphMsdf(glyph->m_bitmapData, dest);''')
        return replace(t, '    int32_t msdf = this->m_theFace && this->m_theFace->UsesMsdf();',
                       '    int32_t msdf = this->m_theFace && (this->m_theFace->UsesMsdf() || this->m_theFace->UsesSlug());')
    edit('Engine/Source/Gxu/GxuFontTextureCache.cpp', cache)

    def geometry(t):
        t = replace(t, '    bool msdf = this->m_currentFace->UsesMsdf();',
                    '    const bool slug = this->m_currentFace->UsesSlug();\n    bool msdf = this->m_currentFace->UsesMsdf();')
        # Slug quads carry normalized outline bounds independently of their data
        # cell. Bitmap pixel rounding/insets and baked outlines do not apply.
        t = t.replace('!msdf &&', '!msdf && !slug &&')
        t = replace(t, 'float width = desc->m_bitmapData.m_glyphCellWidth * scale;',
                    'float width = (slug ? desc->m_bitmapData.m_glyphWidth : desc->m_bitmapData.m_glyphCellWidth) * scale;')
        t = replace(t, '                    if (!(this->m_flags & 0x80)) {\n                        width = floor(width);',
                    '                    if (!(this->m_flags & 0x80) && !slug) {\n                        width = floor(width);')
        t = replace(t, '                    if (msdf) {\n                        quadHeight =',
                    '                    if (slug) quadHeight = desc->m_bitmapData.m_glyphHeight * scale;\n                    if (msdf) {\n                        quadHeight =')
        return replace(t, 'if ((this->m_flags & 0x80) && !msdf)', 'if ((this->m_flags & 0x80) && !msdf && !slug)')
    edit('Engine/Source/Gxu/GxuFontString.cpp', geometry)
    edit('Engine/Source/Gxu/GxuFontMiscClasses.cpp', lambda t: replace(t,
        '        if (desc->m_face->UsesMsdf()) {', '''        if (desc->m_face->UsesSlug()) {
            GxRsSet(GxRs_PixelShader, nullptr);
            GxRsSet(GxRs_Lighting, 0);
            GxRsSet(GxRs_AlphaOp0, 0);
            GxRsSet(GxRs_ColorOp0, (desc->m_face->m_fontFlags & 8) ? 11 : ((desc->m_face->m_fontFlags & 1) ? 10 : 9));
        } else if (desc->m_face->UsesMsdf()) {'''))
    def shader(t):
        t = replace(t, 'float MsdfMedian(', '#include "Slug.glsl"\n\nfloat MsdfMedian(')
        return replace(t, '    int colorOp = u.stageColorOp.x;', '''    int colorOp = u.stageColorOp.x;
    if (colorOp >= 9 && colorOp <= 11) {
        vec4 color = SlugColor(uv, colorOp);
        if (u.flags.w > 0 && color.a * 255.0 < float(u.flags.w)) discard;
        color.rgb = mix(u.fogColor.rgb, color.rgb, inFog);
        outColor = color;
        return;
    }''')
    edit(shaders + 'Ffp.frag', shader)
