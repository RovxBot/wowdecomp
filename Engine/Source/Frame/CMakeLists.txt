wowdecomp_collect_sources(PRIVATE_SOURCES)

add_library(Frame STATIC
    ${PRIVATE_SOURCES}
)

target_include_directories(Frame
    PUBLIC
        ${CMAKE_SOURCE_DIR}
)

target_link_libraries(Frame
    PUBLIC
        DivxDecoder
        freetype
)

wowdecomp_configure_windows_target(Frame)
