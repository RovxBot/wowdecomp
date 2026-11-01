# Same 12340 connection class/packets, with POSIX readiness under Event ownership.
add_library(WowNativeNet STATIC
    ../../../WoW/Source/WowServices/WowConnection.cpp
    ../../../WoW/Source/WowServices/Portable/WowConnection.cpp
    ../../../WoW/Source/WowServices/Portable/WowConnectionNet.cpp
    ../Os/Portable/NetAddress.cpp
    ../Storm/Source/MD5.cpp
    ../Storm/Source/SBig.cpp
    ../../../WoW/Source/WowServices/BigInteger.cpp
    ../../../WoW/Source/WowServices/SRP6_Client.cpp
    ../../../WoW/Source/WowServices/SRP6_Random.cpp
    ../../../WoW/Source/WowServices/Login.cpp
    ../../../WoW/Source/WowServices/Grunt.cpp
    ../../../WoW/Source/WowServices/GruntLogin.cpp
    ../../../WoW/Source/WowServices/Portable/Session.cpp
    ../../../WoW/Source/Net/NetClient/NetClient.cpp)
target_compile_definitions(WowNativeNet PUBLIC WOWDECOMP_NATIVE_CONNECTION=1)
target_link_libraries(WowNativeNet PUBLIC WowNativePackets)
add_executable(NativeConnectionCheck ${PROJECT_SOURCE_DIR}/Tools/NativeNet/connections.cpp)
target_link_libraries(NativeConnectionCheck PRIVATE WowNativeNet)
if(BUILD_TESTING)
    add_test(NAME native-original-connections COMMAND NativeConnectionCheck)
    set_tests_properties(native-original-connections PROPERTIES TIMEOUT 45)
endif()

add_executable(NativeSessionCheck ${PROJECT_SOURCE_DIR}/Tools/NativeNet/sessions.cpp)
target_link_libraries(NativeSessionCheck PRIVATE WowNativeNet)
if(BUILD_TESTING)
    add_test(NAME native-original-sessions COMMAND NativeSessionCheck)
    set_tests_properties(native-original-sessions PROPERTIES TIMEOUT 60)
endif()
