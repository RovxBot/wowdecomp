wowdecomp_collect_sources(PRIVATE_SOURCES)

add_library(SteelseriesMouse STATIC
    ${PRIVATE_SOURCES}
)

target_include_directories(SteelseriesMouse
    PUBLIC
        ${CMAKE_SOURCE_DIR}
)

target_link_libraries(SteelseriesMouse
    PUBLIC
        hid
        setupapi
)

wowdecomp_configure_windows_target(SteelseriesMouse)
