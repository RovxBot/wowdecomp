wowdecomp_collect_sources(PRIVATE_SOURCES)

add_library(Glue STATIC
    ${PRIVATE_SOURCES}
)

target_include_directories(Glue
    PUBLIC
        ${CMAKE_SOURCE_DIR}
)

wowdecomp_configure_windows_target(Glue)
