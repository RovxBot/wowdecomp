include(FetchContent)
# Official SDL 3.2.28 source archive; review before changing either pin.
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
# M2 uses only the software diagnostic renderer. Gx/audio backends remain gated.
foreach(feature AUDIO JOYSTICK HAPTIC SENSOR CAMERA POWER GPU OPENGL OPENGLES VULKAN
                RENDER_D3D RENDER_D3D11 RENDER_D3D12 RENDER_METAL RENDER_VULKAN RENDER_GPU)
    set(SDL_${feature} OFF CACHE BOOL "" FORCE)
endforeach()
FetchContent_Declare(SDL3
    URL https://codeload.github.com/libsdl-org/SDL/tar.gz/7f3ae3d57459e59943a4ecfefc8f6277ec6bf540
    URL_HASH SHA256=7a8347c770b90b33daac2352858ca03f9c9a2ccc8ce711054870361d1a6b32e5
)
FetchContent_MakeAvailable(SDL3)
target_link_libraries(SDL3-static PRIVATE wowdecomp_platform)
target_link_libraries(SDL_uclibc PRIVATE wowdecomp_platform)
if(WOWDECOMP_CORE_SANITIZERS)
    target_compile_options(SDL3-static PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
endif()
add_library(WowNativeWindow STATIC ../Os/Portable/Window.cpp)
target_compile_definitions(WowNativeWindow PUBLIC SDL_MAIN_HANDLED=1)
target_link_libraries(WowNativeWindow PUBLIC WowNativeAsync SDL3::SDL3-static)
if(BUILD_TESTING)
    add_executable(NativeWindowCheck ${PROJECT_SOURCE_DIR}/Tools/PortableWindow/window.cpp)
    target_link_libraries(NativeWindowCheck PRIVATE WowNativeWindow)
    add_test(NAME native-window-input COMMAND NativeWindowCheck)
    set_tests_properties(native-window-input PROPERTIES TIMEOUT 45 ENVIRONMENT "SDL_VIDEODRIVER=dummy")
endif()
