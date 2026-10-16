wowdecomp_collect_sources(PRIVATE_SOURCES)

add_library(ConsoleBase STATIC
    ${PRIVATE_SOURCES}
)

target_include_directories(ConsoleBase
    PUBLIC
        ${CMAKE_SOURCE_DIR}
)

wowdecomp_configure_windows_target(ConsoleBase)
