wowdecomp_collect_sources(PRIVATE_SOURCES)

add_library(Services STATIC
    ${PRIVATE_SOURCES}
)

target_include_directories(Services
    PUBLIC
        ${CMAKE_SOURCE_DIR}
)

target_link_libraries(Services
    PUBLIC
        freetype
)

wowdecomp_configure_windows_target(Services)
