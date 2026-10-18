# Separate native ABI; no legacy Gx/CGxDevice, WGL, D3D or vendor SDK linkage.
add_library(WowNativeGl STATIC
    ../Gx/Portable/GlContext.cpp
    ../Gx/Portable/GlProgram.cpp
    ../Gx/Portable/ShaderPackage.cpp
)
target_link_libraries(WowNativeGl PUBLIC WowNativeWindow)
add_executable(NativeGlCheck ${PROJECT_SOURCE_DIR}/Tools/NativeGx/main.cpp)
target_link_libraries(NativeGlCheck PRIVATE WowNativeGl)
if(WOWDECOMP_CORE_SANITIZERS)
    target_compile_options(WowNativeGl PRIVATE -g1)
    target_compile_options(NativeGlCheck PRIVATE -g1)
endif()
if(BUILD_TESTING)
    add_executable(ShaderPackageCheck ${PROJECT_SOURCE_DIR}/Tools/NativeGx/shaders.cpp)
    target_link_libraries(ShaderPackageCheck PRIVATE WowNativeGl)
    add_test(NAME native-shader-packages COMMAND ShaderPackageCheck)
    add_test(NAME native-gl-compatibility COMMAND NativeGlCheck --cycles 3 --frames 2 --hidden)
    set_tests_properties(native-gl-compatibility PROPERTIES TIMEOUT 60 ENVIRONMENT "SDL_VIDEODRIVER=x11;EGL_PLATFORM=x11")
    add_test(NAME native-gl-unavailable COMMAND NativeGlCheck --expect-unavailable)
    set_tests_properties(native-gl-unavailable PROPERTIES TIMEOUT 30 ENVIRONMENT "SDL_VIDEODRIVER=dummy")
endif()
