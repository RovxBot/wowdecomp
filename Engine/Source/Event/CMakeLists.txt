wowdecomp_collect_sources(PRIVATE_SOURCES)

add_library(Event STATIC
    ${PRIVATE_SOURCES}
)

target_include_directories(Event
    PUBLIC
        ${CMAKE_SOURCE_DIR}
)

wowdecomp_configure_windows_target(Event)
