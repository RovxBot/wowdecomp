wowdecomp_collect_sources(PRIVATE_SOURCES)

add_library(Minigame STATIC
    ${PRIVATE_SOURCES}
)

target_include_directories(Minigame
    PUBLIC
        ${CMAKE_SOURCE_DIR}
)

wowdecomp_configure_windows_target(Minigame)
