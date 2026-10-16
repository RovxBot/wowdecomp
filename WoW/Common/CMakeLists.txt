wowdecomp_collect_sources(PRIVATE_SOURCES)

add_library(WoWCommon STATIC
    ${PRIVATE_SOURCES}
)

target_include_directories(WoWCommon
    PUBLIC
        ${CMAKE_SOURCE_DIR}
)

wowdecomp_configure_windows_target(WoWCommon)
