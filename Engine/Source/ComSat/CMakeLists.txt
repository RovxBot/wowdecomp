wowdecomp_collect_sources(PRIVATE_SOURCES)

add_library(ComSat STATIC
    ${PRIVATE_SOURCES}
)

target_include_directories(ComSat
    PUBLIC
        ${CMAKE_SOURCE_DIR}
)

wowdecomp_configure_windows_target(ComSat)
