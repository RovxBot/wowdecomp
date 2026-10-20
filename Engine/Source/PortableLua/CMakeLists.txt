# Product-modified Lua 5.1.1, compiled as C++ like the Windows target. Only the
# libraries opened by FrameScript_Initialize are selected; no host Lua lookup.
set(lua_sources
    lapi lauxlib lbaselib lbitlib lcode ldebug ldo ldump lfunc lgc llex
    lmathlib lmem lobject lopcodes lparser lstate lstring lstrlib ltable
    ltablib ltm lundump lvm lzio)
set(lua_paths)
foreach(name IN LISTS lua_sources)
    list(APPEND lua_paths ../lua-5.1/src/${name}.c)
endforeach()
set_source_files_properties(${lua_paths} PROPERTIES LANGUAGE CXX)
add_library(WowNativeLua STATIC ${lua_paths}
    ../lua-5.1/src/lmemPool.cpp ../Os/Portable/FunctionPointer.cpp)
target_link_libraries(WowNativeLua PUBLIC WowCoreServices PRIVATE ${CMAKE_DL_LIBS})
if(WOWDECOMP_OS STREQUAL "WINDOWS")
    target_compile_definitions(WowNativeLua PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
endif()
add_executable(NativeLuaCheck ${PROJECT_SOURCE_DIR}/Tools/NativeLua/main.cpp)
target_link_libraries(NativeLuaCheck PRIVATE WowNativeLua)
if(BUILD_TESTING)
    add_test(NAME native-product-lua COMMAND NativeLuaCheck)
    set_tests_properties(native-product-lua PROPERTIES TIMEOUT 60)
    add_test(NAME native-lua-callback-rejection
        COMMAND ${CMAKE_COMMAND} -DEXECUTABLE=$<TARGET_FILE:NativeLuaCheck>
        -P ${CMAKE_CURRENT_SOURCE_DIR}/RejectCallback.cmake)
    set_tests_properties(native-lua-callback-rejection PROPERTIES TIMEOUT 30)
endif()
