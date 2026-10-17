# Native host-file services only; archives/async/SDL are not linked by this target.
add_library(WowNativeFiles STATIC
    ../BlizzardCore/Portable/File.cpp
    ../BlizzardCore/Portable/FilePaths.cpp
    ../Os/Portable/File.cpp
)
if(WOWDECOMP_OS STREQUAL "WINDOWS")
    target_sources(WowNativeFiles PRIVATE ../BlizzardCore/Portable/FileWindows.cpp)
    target_compile_definitions(WowNativeFiles PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
else()
    target_sources(WowNativeFiles PRIVATE ../BlizzardCore/Portable/FilePosix.cpp)
endif()
target_link_libraries(WowNativeFiles PUBLIC WowCoreServices)
if(BUILD_TESTING)
    add_subdirectory(${PROJECT_SOURCE_DIR}/Tools/PortableFiles ${PROJECT_BINARY_DIR}/Tools/PortableFiles)
endif()
