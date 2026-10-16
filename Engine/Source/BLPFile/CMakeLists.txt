wowdecomp_collect_sources(PRIVATE_SOURCES)

add_library(BLPFile STATIC
    ${PRIVATE_SOURCES}
)

target_include_directories(BLPFile
    PUBLIC
        ${CMAKE_SOURCE_DIR}
)

wowdecomp_configure_windows_target(BLPFile)
