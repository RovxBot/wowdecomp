wowdecomp_collect_sources(PRIVATE_SOURCES)

add_library(Tempest STATIC
    ${PRIVATE_SOURCES}
)

target_include_directories(Tempest
    PUBLIC
        ${CMAKE_SOURCE_DIR}
)

wowdecomp_configure_windows_target(Tempest)
