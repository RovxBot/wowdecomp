wowdecomp_collect_sources(PRIVATE_SOURCES)

add_library(FFX STATIC
    ${PRIVATE_SOURCES}
)

target_include_directories(FFX
    PUBLIC
        ${CMAKE_SOURCE_DIR}
)

wowdecomp_configure_windows_target(FFX)
