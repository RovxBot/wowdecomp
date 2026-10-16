wowdecomp_collect_sources(PRIVATE_SOURCES)

add_library(Scrn STATIC
    ${PRIVATE_SOURCES}
)

target_include_directories(Scrn
    PUBLIC
        ${CMAKE_SOURCE_DIR}
)

target_link_libraries(Scrn
    PRIVATE
        freetype
)

wowdecomp_configure_windows_target(Scrn)
