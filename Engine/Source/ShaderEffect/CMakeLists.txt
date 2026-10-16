wowdecomp_collect_sources(PRIVATE_SOURCES)

add_library(ShaderEffect STATIC
    ${PRIVATE_SOURCES}
)

target_include_directories(ShaderEffect
    PUBLIC
        ${CMAKE_SOURCE_DIR}
)

wowdecomp_configure_windows_target(ShaderEffect)
