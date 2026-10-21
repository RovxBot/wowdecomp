# Original 12340 Frame font/name factories; separate native ABI, Linux GL only.
add_library(WowNativeFrameFonts STATIC
    ../Frame/CScriptObject.cpp
    ../Frame/CScriptObject_Scripts.cpp
    ../Frame/CreateFont.cpp
    ../Frame/CSimpleFont.cpp
    ../Frame/CSimpleFont_Scripts.cpp
    ../Frame/CSimpleFontable.cpp
    ../Frame/CSimpleFontStringAttributes.cpp
    ../Frame/ScriptColor.cpp
    ../Frame/Portable/Fonts.cpp
    ../FrameXML/FontXML.cpp
    ../Tempest/CImVector.cpp)
target_link_libraries(WowNativeFrameFonts PUBLIC WowNativeLayout WowNativeGxu)
add_executable(NativeFrameFontsCheck ${PROJECT_SOURCE_DIR}/Tools/NativeFrameFonts/main.cpp)
target_link_libraries(NativeFrameFontsCheck PRIVATE WowNativeFrameFonts)
if(WOWDECOMP_CORE_SANITIZERS)
    target_compile_options(WowNativeFrameFonts PRIVATE -g1)
    target_compile_options(NativeFrameFontsCheck PRIVATE -g1)
endif()
if(BUILD_TESTING)
    add_test(NAME native-frame-font-factories COMMAND NativeFrameFontsCheck
        --font ${PROJECT_SOURCE_DIR}/Tools/NativeGx/fonts/cache.ttf)
    set_tests_properties(native-frame-font-factories PROPERTIES TIMEOUT 120
        ENVIRONMENT "SDL_VIDEODRIVER=x11;EGL_PLATFORM=x11")
endif()
