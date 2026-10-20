# Original 12340 FrameScript consumers in the native ABI, never host Lua.
add_library(WowNativeFrameScript STATIC
    ../FrameScript/FrameScript.cpp
    ../FrameScript/FrameScript_Object.cpp
    ../FrameScript/FrameScript_Scripts.cpp
    ../FrameScript/Portable/Runtime.cpp
    ../Base/Status.cpp)
target_link_libraries(WowNativeFrameScript PUBLIC WowNativeLua WowNativeFiles WowPortableCore)
add_executable(NativeFrameScriptCheck ${PROJECT_SOURCE_DIR}/Tools/NativeFrameScript/main.cpp)
target_link_libraries(NativeFrameScriptCheck PRIVATE WowNativeFrameScript WowNativeAsync)
if(BUILD_TESTING)
    add_test(NAME native-framescript COMMAND NativeFrameScriptCheck)
    set_tests_properties(native-framescript PROPERTIES TIMEOUT 60)
endif()
