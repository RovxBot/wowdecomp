wowdecomp_collect_sources(PRIVATE_SOURCES)

add_library(Console STATIC
    ${PRIVATE_SOURCES}
)

target_include_directories(Console
    PUBLIC
        ${CMAKE_SOURCE_DIR}
)

target_link_libraries(Console
    PRIVATE
        freetype
)

wowdecomp_configure_windows_target(Console)
