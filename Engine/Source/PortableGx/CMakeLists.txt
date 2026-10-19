# Native ABI with selected common legacy Gx dispatch; no WGL/D3D/vendor SDK linkage.
add_library(WowNativeGl STATIC
    ../Gx/Portable/LegacyDevice.cpp
    ../Gx/Gx.cpp
    ../Gx/Buffer.cpp
    ../Gx/CGxDevice/CGxDevice.cpp
    ../Gx/CGxDevice/CGxShader.cpp
    ../Gx/CGxDevice/EmergencyMem.cpp
    ../Gx/CGxTex.cpp
    ../Gx/CGxTexFlags.cpp
    ../Gx/CGxPool.cpp
    ../Gx/CGxBuf.cpp
    ../Gx/CGxFormat.cpp
    ../Gx/CGxGammaRamp.cpp
    ../Tempest/CiRect.cpp
    ../Tempest/CRect.cpp
    ../Gx/Portable/GlContext.cpp
    ../Gx/Portable/GlBuffers.cpp
    ../Gx/Portable/GlProgram.cpp
    ../Gx/Portable/GlRenderTargets.cpp
    ../Gx/Portable/GlTransforms.cpp
    ../Gx/Portable/ShaderPackage.cpp
    ../Gx/CGxMatrixStack.cpp
    ../Tempest/C44Matrix.cpp
    ../Tempest/C33Matrix.cpp
)
target_link_libraries(WowNativeGl PUBLIC WowNativeWindow)

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

add_executable(NativeGlCheck ${PROJECT_SOURCE_DIR}/Tools/NativeGx/main.cpp)
target_link_libraries(NativeGlCheck PRIVATE WowNativeGl WowNativeFonts)
if(WOWDECOMP_CORE_SANITIZERS)
    target_compile_options(WowNativeGl PRIVATE -g1)
    target_compile_options(NativeGlCheck PRIVATE -g1)
endif()
add_executable(LegacyGxCheck ${PROJECT_SOURCE_DIR}/Tools/NativeGx/legacy.cpp)
target_link_libraries(LegacyGxCheck PRIVATE WowNativeGl WowNativeFonts)
if(WOWDECOMP_CORE_SANITIZERS)
    target_compile_options(LegacyGxCheck PRIVATE -g1)
endif()
if(BUILD_TESTING)
    add_test(NAME native-legacy-gx-dispatch COMMAND LegacyGxCheck)
    set_tests_properties(native-legacy-gx-dispatch PROPERTIES TIMEOUT 60 ENVIRONMENT "SDL_VIDEODRIVER=x11;EGL_PLATFORM=x11")
    add_executable(NativeFontsCheck ${PROJECT_SOURCE_DIR}/Tools/NativeGx/fonts.cpp)
    target_link_libraries(NativeFontsCheck PRIVATE WowNativeFonts)
    target_include_directories(NativeFontsCheck PRIVATE ${PROJECT_SOURCE_DIR}/ThirdParty/freetype/include)
    add_test(NAME native-original-font-raster COMMAND NativeFontsCheck ${PROJECT_SOURCE_DIR}/Tools/NativeGx/fonts/tiny.ttf)
    set_tests_properties(native-original-font-raster PROPERTIES TIMEOUT 30)
    add_executable(GlTexturesCheck ${PROJECT_SOURCE_DIR}/Tools/NativeGx/textures.cpp)
    target_link_libraries(GlTexturesCheck PRIVATE WowNativeGl)
    if(WOWDECOMP_CORE_SANITIZERS)
        target_compile_options(GlTexturesCheck PRIVATE -g1)
    endif()
    add_test(NAME native-gl-textures COMMAND GlTexturesCheck)
    set_tests_properties(native-gl-textures PROPERTIES TIMEOUT 60 ENVIRONMENT "SDL_VIDEODRIVER=x11;EGL_PLATFORM=x11")
    add_executable(GlBuffersCheck ${PROJECT_SOURCE_DIR}/Tools/NativeGx/buffers.cpp)
    target_link_libraries(GlBuffersCheck PRIVATE WowNativeGl)
    if(WOWDECOMP_CORE_SANITIZERS)
        target_compile_options(GlBuffersCheck PRIVATE -g1)
    endif()
    add_test(NAME native-gl-buffers COMMAND GlBuffersCheck)
    set_tests_properties(native-gl-buffers PROPERTIES TIMEOUT 60 ENVIRONMENT "SDL_VIDEODRIVER=x11;EGL_PLATFORM=x11")
    add_executable(GlTransformsCheck ${PROJECT_SOURCE_DIR}/Tools/NativeGx/transforms.cpp)
    target_link_libraries(GlTransformsCheck PRIVATE WowNativeGl)
    if(WOWDECOMP_CORE_SANITIZERS)
        target_compile_options(GlTransformsCheck PRIVATE -g1)
    endif()
    add_test(NAME native-gl-transforms COMMAND GlTransformsCheck)
    set_tests_properties(native-gl-transforms PROPERTIES TIMEOUT 60 ENVIRONMENT "SDL_VIDEODRIVER=x11;EGL_PLATFORM=x11")
    add_executable(GlRenderTargetsCheck ${PROJECT_SOURCE_DIR}/Tools/NativeGx/targets.cpp)
    target_link_libraries(GlRenderTargetsCheck PRIVATE WowNativeGl)
    if(WOWDECOMP_CORE_SANITIZERS)
        target_compile_options(GlRenderTargetsCheck PRIVATE -g1)
    endif()
    add_test(NAME native-gl-render-targets COMMAND GlRenderTargetsCheck)
    set_tests_properties(native-gl-render-targets PROPERTIES TIMEOUT 60 ENVIRONMENT "SDL_VIDEODRIVER=x11;EGL_PLATFORM=x11")
    add_executable(ShaderPackageCheck ${PROJECT_SOURCE_DIR}/Tools/NativeGx/shaders.cpp)
    target_link_libraries(ShaderPackageCheck PRIVATE WowNativeGl)
    add_test(NAME native-shader-packages COMMAND ShaderPackageCheck)
    add_test(NAME native-gl-compatibility COMMAND NativeGlCheck --cycles 3 --frames 2 --hidden)
    set_tests_properties(native-gl-compatibility PROPERTIES TIMEOUT 60 ENVIRONMENT "SDL_VIDEODRIVER=x11;EGL_PLATFORM=x11")
    add_test(NAME native-gl-unavailable COMMAND NativeGlCheck --expect-unavailable)
    set_tests_properties(native-gl-unavailable PROPERTIES TIMEOUT 30 ENVIRONMENT "SDL_VIDEODRIVER=dummy")
endif()
