wowdecomp_collect_sources(PRIVATE_SOURCES)

add_library(LiquidSystem STATIC
    ${PRIVATE_SOURCES}
)

target_include_directories(LiquidSystem
    PUBLIC
        ${CMAKE_SOURCE_DIR}
)

wowdecomp_configure_windows_target(LiquidSystem)
