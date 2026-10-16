wowdecomp_collect_sources(PRIVATE_SOURCES)

add_library(UIUtil STATIC
    ${PRIVATE_SOURCES}
)

target_include_directories(UIUtil
    PUBLIC
        ${CMAKE_SOURCE_DIR}
)

wowdecomp_configure_windows_target(UIUtil)
