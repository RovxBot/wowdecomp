wowdecomp_collect_sources(PRIVATE_SOURCES)

add_library(pcreposix STATIC
    ${PRIVATE_SOURCES}
)

target_include_directories(pcreposix
    PUBLIC
        ${CMAKE_SOURCE_DIR}
)

target_link_libraries(pcreposix
    PRIVATE
        pcre
)

wowdecomp_configure_windows_target(pcreposix)
