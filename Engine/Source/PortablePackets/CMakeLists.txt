# Existing 12340 packet buffers/cipher/digest code in the native ABI. This is a
# prerequisite for WowConnection, not a socket, authentication or gameplay target.
add_library(WowNativePackets STATIC
    ../Base/CDataStore.cpp
    ../Storm/Source/SArc4.cpp
    ../BlizzardCore/Source/Packages/Util/SHA1.cpp)
target_link_libraries(WowNativePackets PUBLIC WowCoreServices)
add_executable(NativePacketCheck ${PROJECT_SOURCE_DIR}/Tools/NativeNet/packets.cpp)
target_link_libraries(NativePacketCheck PRIVATE WowNativePackets)
if(BUILD_TESTING)
    add_test(NAME native-12340-packets COMMAND NativePacketCheck)
    set_tests_properties(native-12340-packets PROPERTIES TIMEOUT 30)
endif()
