wowdecomp_collect_sources(PRIVATE_SOURCES)

add_library(Game STATIC
    ${PRIVATE_SOURCES}
)

target_include_directories(Game
    PUBLIC
        ${CMAKE_SOURCE_DIR}
)

target_link_libraries(Game
    PUBLIC
        pcreposix
        freetype
)

wowdecomp_configure_windows_target(Game)
