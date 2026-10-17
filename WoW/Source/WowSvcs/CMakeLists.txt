wowdecomp_collect_sources(PRIVATE_SOURCES)

add_library(WowSvcs STATIC
    ${PRIVATE_SOURCES}
)

target_include_directories(WowSvcs
    PUBLIC
        ${CMAKE_SOURCE_DIR}
)

wowdecomp_configure_windows_target(WowSvcs)
