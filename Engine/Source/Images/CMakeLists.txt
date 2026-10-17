wowdecomp_collect_sources(PRIVATE_SOURCES)

add_library(Images STATIC
    ${PRIVATE_SOURCES}
)

target_include_directories(Images
    PUBLIC
        ${CMAKE_SOURCE_DIR}
)

target_link_libraries(Images
    PRIVATE
        jpeg
)

wowdecomp_configure_windows_target(Images)
