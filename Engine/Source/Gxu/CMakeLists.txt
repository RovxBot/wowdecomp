wowdecomp_collect_sources(PRIVATE_SOURCES)

add_library(Gxu STATIC
    ${PRIVATE_SOURCES}
)

target_include_directories(Gxu
    PUBLIC
        ${CMAKE_SOURCE_DIR}
)

target_link_libraries(Gxu
    PUBLIC
        freetype
)

wowdecomp_configure_windows_target(Gxu)
