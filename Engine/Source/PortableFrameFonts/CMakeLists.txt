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

# Original render batching is a prerequisite for actual region/root consumers.
add_library(WowNativeFrameRender STATIC
    ../Frame/CRenderBatch.cpp
    ../Frame/CSimpleBatchRender.cpp
    ../Frame/Portable/Render.cpp)
target_link_libraries(WowNativeFrameRender PUBLIC WowNativeFrameFonts)
add_library(WowNativeFrameRegions STATIC
    ../Frame/CScriptRegion_Scripts.cpp
    ../Frame/CSimpleFrame_Scripts.cpp
    ../Frame/CSimpleFontString_Scripts.cpp
    ../Frame/CSimpleTitleRegion_Scripts.cpp
    ../Frame/Portable/Scripts.cpp
    ../Frame/CScriptRegion.cpp
    ../Frame/CSimpleRegion.cpp
    ../Storm/Source/SUni.cpp
    ../Frame/Language.cpp
    ../Frame/CSimpleFontString.cpp
    ../Frame/FontStringAttributes.cpp
    ../Frame/FramePixel.cpp
    ../Frame/CSimpleFrame.cpp
    ../Frame/CSimpleTop.cpp
    ../Frame/CSimpleTitleRegion.cpp
    ../Frame/CFrameStrataNode.cpp
    ../Frame/Portable/Hierarchy.cpp
    ../Frame/Portable/WindowEvents.cpp
    ../FrameXML/Portable/Factories.cpp
    ../FrameXML/RegionXML.cpp
    ../FrameXML/CreateFrame.cpp
    ../Event/CEvent.cpp
    ../Event/CMouseEvent.cpp
    ../Services/ScreenProjection.cpp)
target_link_libraries(WowNativeFrameRegions PUBLIC WowNativeFrameRender)
add_executable(NativeFrameHierarchyCheck ${PROJECT_SOURCE_DIR}/Tools/NativeFrameHierarchy/main.cpp)
target_link_libraries(NativeFrameHierarchyCheck PRIVATE WowNativeFrameRegions)
add_executable(NativeFrameRegionsCheck ${PROJECT_SOURCE_DIR}/Tools/NativeFrameRegions/main.cpp)
target_link_libraries(NativeFrameRegionsCheck PRIVATE WowNativeFrameRegions)
add_executable(NativeFrameRenderCheck ${PROJECT_SOURCE_DIR}/Tools/NativeFrameRender/main.cpp)
target_link_libraries(NativeFrameRenderCheck PRIVATE WowNativeFrameRender)
add_executable(NativeFrameFontsCheck ${PROJECT_SOURCE_DIR}/Tools/NativeFrameFonts/main.cpp)
target_link_libraries(NativeFrameFontsCheck PRIVATE WowNativeFrameFonts)
if(WOWDECOMP_CORE_SANITIZERS)
    target_compile_options(WowNativeFrameFonts PRIVATE -g1)
    target_compile_options(NativeFrameFontsCheck PRIVATE -g1)
    target_compile_options(WowNativeFrameRender PRIVATE -g1)
    target_compile_options(NativeFrameRenderCheck PRIVATE -g1)
    target_compile_options(NativeFrameHierarchyCheck PRIVATE -g1)
    target_compile_options(WowNativeFrameRegions PRIVATE -g1)
    target_compile_options(NativeFrameRegionsCheck PRIVATE -g1)
endif()
if(BUILD_TESTING)
    add_test(NAME native-frame-sdl-mouse COMMAND NativeFrameHierarchyCheck
        --font ${PROJECT_SOURCE_DIR}/Tools/NativeGx/fonts/moves.ttf --mouse-events)
    set_tests_properties(native-frame-sdl-mouse PROPERTIES TIMEOUT 120
        ENVIRONMENT "SDL_VIDEODRIVER=x11;EGL_PLATFORM=x11")
    add_test(NAME native-frame-sdl-ime COMMAND NativeFrameHierarchyCheck
        --font ${PROJECT_SOURCE_DIR}/Tools/NativeGx/fonts/moves.ttf --ime-events)
    set_tests_properties(native-frame-sdl-ime PROPERTIES TIMEOUT 120
        ENVIRONMENT "SDL_VIDEODRIVER=x11;EGL_PLATFORM=x11")
    add_test(NAME native-frame-sdl-keyboard COMMAND NativeFrameHierarchyCheck
        --font ${PROJECT_SOURCE_DIR}/Tools/NativeGx/fonts/moves.ttf --keyboard-events)
    set_tests_properties(native-frame-sdl-keyboard PROPERTIES TIMEOUT 120
        ENVIRONMENT "SDL_VIDEODRIVER=x11;EGL_PLATFORM=x11")
    add_test(NAME native-frame-sdl-size COMMAND NativeFrameHierarchyCheck
        --font ${PROJECT_SOURCE_DIR}/Tools/NativeGx/fonts/moves.ttf --window-events)
    set_tests_properties(native-frame-sdl-size PROPERTIES TIMEOUT 120
        ENVIRONMENT "SDL_VIDEODRIVER=x11;EGL_PLATFORM=x11")
    add_test(NAME native-frame-parent-root COMMAND NativeFrameHierarchyCheck
        --font ${PROJECT_SOURCE_DIR}/Tools/NativeGx/fonts/moves.ttf)
    set_tests_properties(native-frame-parent-root PROPERTIES TIMEOUT 120
        ENVIRONMENT "SDL_VIDEODRIVER=x11;EGL_PLATFORM=x11")
    add_test(NAME native-frame-font-string-regions COMMAND NativeFrameRegionsCheck
        --font ${PROJECT_SOURCE_DIR}/Tools/NativeGx/fonts/moves.ttf)
    set_tests_properties(native-frame-font-string-regions PROPERTIES TIMEOUT 120
        ENVIRONMENT "SDL_VIDEODRIVER=x11;EGL_PLATFORM=x11")
    add_test(NAME native-frame-render-batches COMMAND NativeFrameRenderCheck
        --font ${PROJECT_SOURCE_DIR}/Tools/NativeGx/fonts/moves.ttf)
    set_tests_properties(native-frame-render-batches PROPERTIES TIMEOUT 120
        ENVIRONMENT "SDL_VIDEODRIVER=x11;EGL_PLATFORM=x11")
    add_test(NAME native-frame-font-factories COMMAND NativeFrameFontsCheck
        --font ${PROJECT_SOURCE_DIR}/Tools/NativeGx/fonts/moves.ttf)
    set_tests_properties(native-frame-font-factories PROPERTIES TIMEOUT 120
        ENVIRONMENT "SDL_VIDEODRIVER=x11;EGL_PLATFORM=x11")
endif()
