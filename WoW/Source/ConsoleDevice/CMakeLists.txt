wowdecomp_collect_sources(PRIVATE_SOURCES)

add_library(ConsoleDevice STATIC
    ${PRIVATE_SOURCES}
)

target_include_directories(ConsoleDevice
    PUBLIC
        ${CMAKE_SOURCE_DIR}
)

wowdecomp_configure_windows_target(ConsoleDevice)
