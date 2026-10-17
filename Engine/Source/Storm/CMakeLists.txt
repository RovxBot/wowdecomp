wowdecomp_collect_sources(PRIVATE_SOURCES)

add_library(Storm STATIC
    ${PRIVATE_SOURCES}
)

target_include_directories(Storm
    PUBLIC
        ${CMAKE_SOURCE_DIR}
)

target_link_libraries(Storm
    PUBLIC
        blast
        bzip2
        zlib
)

wowdecomp_configure_windows_target(Storm)
