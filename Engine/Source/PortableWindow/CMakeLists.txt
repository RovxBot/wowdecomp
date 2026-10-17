include(FetchContent)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()
# Official SDL 3.2.28 source archive; review before changing either pin.
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
# M2 uses only the software diagnostic renderer. Gx/audio backends remain gated.
foreach(feature AUDIO JOYSTICK HAPTIC SENSOR CAMERA POWER GPU OPENGL OPENGLES VULKAN
                RENDER_D3D RENDER_D3D11 RENDER_D3D12 RENDER_METAL RENDER_VULKAN RENDER_GPU
                HIDAPI HIDAPI_JOYSTICK VIRTUAL_JOYSTICK)
    set(SDL_${feature} OFF CACHE BOOL "" FORCE)
endforeach()
FetchContent_Declare(SDL3
    URL https://codeload.github.com/libsdl-org/SDL/tar.gz/7f3ae3d57459e59943a4ecfefc8f6277ec6bf540
    URL_HASH SHA256=7a8347c770b90b33daac2352858ca03f9c9a2ccc8ce711054870361d1a6b32e5
)
FetchContent_MakeAvailable(SDL3)
# SDL exports its own static target. Give its compilation explicit identities
# without adding a product-only interface target to SDL's export dependency set.
foreach(target SDL3-static SDL_uclibc)
    target_compile_definitions(${target} PRIVATE
        WOWDECOMP_OS_${WOWDECOMP_OS}=1
        WOWDECOMP_CPU_${WOWDECOMP_CPU}=1
        WOWDECOMP_ARCH_${WOWDECOMP_POINTER_BITS}=1)
endforeach()
if(WOWDECOMP_CORE_SANITIZERS)
    target_compile_options(SDL3-static PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
endif()
add_library(WowNativeWindow STATIC ../Os/Portable/Window.cpp ../Services/Portable/ShellSettings.cpp)
target_compile_definitions(WowNativeWindow PUBLIC SDL_MAIN_HANDLED=1)
target_link_libraries(WowNativeWindow PUBLIC WowNativeAsync SDL3::SDL3-static)
add_executable(NativeShell ${PROJECT_SOURCE_DIR}/Tools/NativeShell/main.cpp)
target_link_libraries(NativeShell PRIVATE WowNativeWindow)
if(BUILD_TESTING)
    add_executable(NativeWindowCheck ${PROJECT_SOURCE_DIR}/Tools/PortableWindow/window.cpp)
    target_link_libraries(NativeWindowCheck PRIVATE WowNativeWindow)
    add_test(NAME native-window-input COMMAND NativeWindowCheck)
    set_tests_properties(native-window-input PROPERTIES TIMEOUT 45 ENVIRONMENT "SDL_VIDEODRIVER=dummy")
    add_executable(ShellSettingsCheck ${PROJECT_SOURCE_DIR}/Tools/PortableWindow/settings.cpp)
    target_link_libraries(ShellSettingsCheck PRIVATE WowNativeWindow)
    add_test(NAME native-shell-settings COMMAND ShellSettingsCheck)
    set_tests_properties(native-shell-settings PROPERTIES TIMEOUT 30)
endif()
