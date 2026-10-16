wowdecomp_collect_sources(PRIVATE_SOURCES)

add_library(Battlenet STATIC
    ${PRIVATE_SOURCES}
)

target_include_directories(Battlenet
    PUBLIC
        ${CMAKE_SOURCE_DIR}
)

wowdecomp_configure_windows_target(Battlenet)
