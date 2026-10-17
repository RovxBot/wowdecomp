wowdecomp_collect_sources(PRIVATE_SOURCES)

add_library(FrameXML STATIC
    ${PRIVATE_SOURCES}
)

target_include_directories(FrameXML
    PUBLIC
        ${CMAKE_SOURCE_DIR}
)

target_link_libraries(FrameXML
    PUBLIC
        expat
)

wowdecomp_configure_windows_target(FrameXML)
