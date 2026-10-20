# Original XML tree/Storm pools and bundled Expat, in the native ABI only.
add_library(WowNativeExpat STATIC
    ../../../ThirdParty/expat/lib/xmlparse.c
    ../../../ThirdParty/expat/lib/xmlrole.c
    ../../../ThirdParty/expat/lib/xmltok.c)
target_include_directories(WowNativeExpat PUBLIC ${PROJECT_SOURCE_DIR}/ThirdParty/expat/lib
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(WowNativeExpat PUBLIC _STATIC)
target_link_libraries(WowNativeExpat PUBLIC WowCoreServices)
add_library(WowNativeXML STATIC
    ../FrameXML/XMLTree.cpp
    ../FrameXML/Portable/Runtime.cpp
    ../FrameXML/Portable/Signature.cpp
    ../Base/CDataAllocator.cpp
    ../Base/RCString.cpp
    ../Base/RefCount.cpp)
target_link_libraries(WowNativeXML PUBLIC WowNativeFrameScript WowNativeExpat WowNativeAssets)
add_executable(NativeXMLCheck ${PROJECT_SOURCE_DIR}/Tools/NativeXML/main.cpp)
target_link_libraries(NativeXMLCheck PRIVATE WowNativeXML WowNativeAsync)
if(BUILD_TESTING)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_test(NAME native-xml-fixture COMMAND ${Python3_EXECUTABLE}
        ${PROJECT_SOURCE_DIR}/Tools/NativeXML/generate_fixtures.py ${CMAKE_CURRENT_BINARY_DIR}/fixtures)
    set_tests_properties(native-xml-fixture PROPERTIES FIXTURES_SETUP xml-fixture)
    add_test(NAME native-xml-signatures COMMAND NativeXMLCheck ${CMAKE_CURRENT_BINARY_DIR}/fixtures)
    set_tests_properties(native-xml-signatures PROPERTIES FIXTURES_REQUIRED xml-fixture TIMEOUT 120)
endif()
