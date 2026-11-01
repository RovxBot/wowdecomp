# Same 12340 connection class/packets, with POSIX readiness under Event ownership.
add_library(WowNativeNet STATIC
    ../../../WoW/Source/WowServices/WowConnection.cpp
    ../../../WoW/Source/WowServices/Portable/WowConnection.cpp
    ../../../WoW/Source/WowServices/Portable/WowConnectionNet.cpp
    ../Os/Portable/NetAddress.cpp)
target_compile_definitions(WowNativeNet PUBLIC WOWDECOMP_NATIVE_CONNECTION=1)
target_link_libraries(WowNativeNet PUBLIC WowNativePackets)
add_executable(NativeConnectionCheck ${PROJECT_SOURCE_DIR}/Tools/NativeNet/connections.cpp)
target_link_libraries(NativeConnectionCheck PRIVATE WowNativeNet)
if(BUILD_TESTING)
    add_test(NAME native-original-connections COMMAND NativeConnectionCheck)
    set_tests_properties(native-original-connections PROPERTIES TIMEOUT 45)
endif()
