wowdecomp_collect_sources(PRIVATE_SOURCES)

add_library(Magic STATIC
    ${PRIVATE_SOURCES}
)

target_include_directories(Magic
    PUBLIC
        ${CMAKE_SOURCE_DIR}
)

wowdecomp_configure_windows_target(Magic)
