# Original 12340 anchors/layout and XML coordinate contracts, without graphics.
add_library(WowNativeLayout STATIC
    ../Frame/CLayoutFrame.cpp
    ../Frame/CFramePoint.cpp
    ../Frame/Portable/Runtime.cpp
    ../FrameXML/LayoutXML.cpp
    ../Base/Coordinate.cpp
    ../Tempest/CRect.cpp
    ../Tempest/C2Vector.cpp)
target_link_libraries(WowNativeLayout PUBLIC WowNativeXML)
add_executable(NativeLayoutCheck ${PROJECT_SOURCE_DIR}/Tools/NativeLayout/main.cpp)
target_link_libraries(NativeLayoutCheck PRIVATE WowNativeLayout)
if(BUILD_TESTING)
    add_test(NAME native-layout COMMAND NativeLayoutCheck)
    set_tests_properties(native-layout PROPERTIES TIMEOUT 60)
endif()
