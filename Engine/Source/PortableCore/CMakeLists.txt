# Audit each source before adding it: these targets do not include game, archive,
# graphics, network, audio or optional device services.
find_package(Threads REQUIRED)
add_library(WowPortableCore STATIC ../Storm/Source/MD5.cpp ../Storm/Source/Sha1.cpp)
target_include_directories(WowPortableCore PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(WowPortableCore PUBLIC wowdecomp_platform)

add_library(WowCoreServices STATIC
    ../Os/Portable/Sync.cpp
    ../Os/Portable/Tls.cpp
    ../Os/Portable/Time.cpp
    ../Os/Time.cpp
    ../Storm/Portable/SMem.cpp
    ../Storm/Portable/LogError.cpp
    ../Storm/Portable/SLock.cpp
    ../Storm/Source/StormRtl.cpp
    ../BlizzardCore/Portable/Core.cpp
    ../BlizzardCore/Portable/Thread.cpp
    ../BlizzardCore/Portable/Time.cpp
)
if(WOWDECOMP_OS STREQUAL "WINDOWS")
    target_sources(WowCoreServices PRIVATE ../Os/Portable/ThreadWindows.cpp ../Os/Portable/RandomWindows.cpp)
    target_link_libraries(WowCoreServices PRIVATE bcrypt)
    target_compile_definitions(WowCoreServices PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
else()
    target_sources(WowCoreServices PRIVATE ../Os/Portable/ThreadPosix.cpp ../Os/Portable/RandomPosix.cpp)
endif()
target_compile_definitions(WowCoreServices PUBLIC WOWDECOMP_PORTABLE_RUNTIME=1)
target_include_directories(WowCoreServices PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(WowCoreServices PUBLIC wowdecomp_platform Threads::Threads)

option(WOWDECOMP_CORE_SANITIZERS "Instrument portable services with ASan/UBSan" OFF)
if(WOWDECOMP_CORE_SANITIZERS)
    if(MSVC)
        message(FATAL_ERROR "Use Clang for the sanitizer core configuration.")
    endif()
    target_compile_options(WowCoreServices PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(WowCoreServices PUBLIC -fsanitize=address,undefined)
endif()

add_subdirectory(${PROJECT_SOURCE_DIR}/Tools/NativeCore ${PROJECT_BINARY_DIR}/Tools/NativeCore)
if(BUILD_TESTING)
    add_subdirectory(${PROJECT_SOURCE_DIR}/Tools/PortableCore ${PROJECT_BINARY_DIR}/Tools/PortableCore)
endif()
