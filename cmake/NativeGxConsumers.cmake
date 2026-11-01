# Bundled product FreeType in the native ABI; keep the Windows freetype target
# separately selected. Sources/version remain product-pinned, no system lookup.
add_library(WowNativeFonts STATIC
    ../Gxu/Portable/FontFace.cpp
    ${PROJECT_SOURCE_DIR}/ThirdParty/freetype/src/base/ftsystem.c
    ${PROJECT_SOURCE_DIR}/ThirdParty/freetype/src/base/ftinit.c
    ${PROJECT_SOURCE_DIR}/ThirdParty/freetype/src/base/ftdebug.c
    ${PROJECT_SOURCE_DIR}/ThirdParty/freetype/src/base/ftbase.c
    ${PROJECT_SOURCE_DIR}/ThirdParty/freetype/src/raster/raster.c
    ${PROJECT_SOURCE_DIR}/ThirdParty/freetype/src/sfnt/sfnt.c
    ${PROJECT_SOURCE_DIR}/ThirdParty/freetype/src/smooth/smooth.c
    ${PROJECT_SOURCE_DIR}/ThirdParty/freetype/src/truetype/truetype.c
 )
target_include_directories(WowNativeFonts PRIVATE
    ${PROJECT_SOURCE_DIR}/Engine/Source/Gxu/Portable/FreeTypeConfig
    ${PROJECT_SOURCE_DIR}/ThirdParty/freetype/include
    ${PROJECT_SOURCE_DIR}/ThirdParty/freetype/src)
target_link_libraries(WowNativeFonts PUBLIC WowCoreServices)

# Original Gxu consumers in the native ABI; no legacy SFile/async service objects.
add_library(WowNativeGxu STATIC
    ../Gxu/Portable/LegacyFonts.cpp
    ../Gxu/Portable/FontTexture.cpp
    ../Gxu/Portable/FontMath.cpp
    ../Gxu/Gxu.cpp
    ../Gxu/GxuFontFace.cpp
    ../Gxu/GxuFontFont.cpp
    ../Gxu/GxuFontMain.cpp
    ../Gxu/GxuFontString.cpp
    ../Gxu/GxuFontTextureCache.cpp
    ../Gxu/GxuFontMiscClasses.cpp
    ../Gxu/GxuFontUtil.cpp
    ../Gxu/IGxuFontGlyph.cpp
    ../Services/TextBlock.cpp
    ../Services/Portable/Texture.cpp
    ../Base/Handle.cpp
    ../Base/Status.cpp
    ../Base/Coordinate.cpp
    ../Base/ConvertUTF.cpp
    ../Tempest/C3Vector.cpp
    ../Tempest/C4Vector.cpp
)
target_include_directories(WowNativeGxu PUBLIC ${PROJECT_SOURCE_DIR}/ThirdParty/freetype/include)
target_link_libraries(WowNativeGxu PUBLIC ${WOWDECOMP_NATIVE_GX_TARGET} WowNativeFonts)
