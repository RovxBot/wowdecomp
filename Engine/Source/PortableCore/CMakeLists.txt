# Deliberate allowlist: these existing Storm routines need no Windows services.
# Growing this target requires auditing each new source and its dependencies.
add_library(WowPortableCore STATIC
    ../Storm/Source/MD5.cpp
    ../Storm/Source/Sha1.cpp
)
target_include_directories(WowPortableCore PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(WowPortableCore PUBLIC wowdecomp_platform)

if(BUILD_TESTING)
    add_subdirectory(${PROJECT_SOURCE_DIR}/Tools/PortableCore ${PROJECT_BINARY_DIR}/Tools/PortableCore)
endif()
