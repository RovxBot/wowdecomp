wowdecomp_collect_sources(PRIVATE_SOURCES)

add_library(FrameScript STATIC
    ${PRIVATE_SOURCES}
)

target_include_directories(FrameScript
    PUBLIC
        ${CMAKE_SOURCE_DIR}
)

wowdecomp_configure_windows_target(FrameScript)
