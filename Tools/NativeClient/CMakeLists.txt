# Integrated startup, using the already qualified native ABI and services.
add_library(WowNativeStartup STATIC
    ${PROJECT_SOURCE_DIR}/Engine/Source/Frame/CSimpleHTML.cpp
    ${PROJECT_SOURCE_DIR}/Engine/Source/Frame/CSimpleHTML_Scripts.cpp
    ${PROJECT_SOURCE_DIR}/Engine/Source/Frame/CSimpleHyperlinkedFrame.cpp
    ${PROJECT_SOURCE_DIR}/Engine/Source/Frame/CSimpleScrollFrame.cpp
    ${PROJECT_SOURCE_DIR}/Engine/Source/Frame/CSimpleScrollFrame_Scripts.cpp
    ${PROJECT_SOURCE_DIR}/Engine/Source/Frame/CSimpleSlider.cpp
    ${PROJECT_SOURCE_DIR}/Engine/Source/Frame/CSimpleSlider_Scripts.cpp
    ${PROJECT_SOURCE_DIR}/Engine/Source/Frame/Portable/LoginWidgets.cpp
    ${PROJECT_SOURCE_DIR}/WoW/Source/Glue/GlueMgr.cpp
    ${PROJECT_SOURCE_DIR}/Engine/Source/ConsoleBase/ConsoleVar.cpp
    ${PROJECT_SOURCE_DIR}/Engine/Source/ConsoleBase/ConsoleCommand.cpp
    ${PROJECT_SOURCE_DIR}/Engine/Source/ConsoleBase/Portable/Console.cpp
    ${PROJECT_SOURCE_DIR}/Engine/Source/Event/Portable/Loop.cpp
    ${PROJECT_SOURCE_DIR}/Engine/Source/FrameXML/Portable/Load.cpp
    ${PROJECT_SOURCE_DIR}/WoW/Source/Glue/Portable/Startup.cpp)
target_link_libraries(WowNativeStartup PUBLIC WowNativeFrameRegions)
add_executable(WowNativeClient main.cpp)
target_link_libraries(WowNativeClient PRIVATE WowNativeStartup)
add_executable(NativeStartupCheck startup_check.cpp)
target_link_libraries(NativeStartupCheck PRIVATE WowNativeStartup)
if(BUILD_TESTING)
    add_test(NAME native-integrated-startup-contracts COMMAND NativeStartupCheck)
endif()
if(WOWDECOMP_CORE_SANITIZERS)
    target_compile_options(WowNativeStartup PRIVATE -g1)
    target_compile_options(WowNativeClient PRIVATE -g1)
endif()
