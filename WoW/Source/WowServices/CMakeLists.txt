wowdecomp_collect_sources(PRIVATE_SOURCES)

add_library(WowServices STATIC
    ${PRIVATE_SOURCES}
)

target_include_directories(WowServices
    PUBLIC
        ${CMAKE_SOURCE_DIR}
)

wowdecomp_configure_windows_target(WowServices)
