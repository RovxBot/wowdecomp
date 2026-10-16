wowdecomp_collect_sources(PRIVATE_SOURCES)

add_library(Base STATIC
    ${PRIVATE_SOURCES}
)

target_include_directories(Base
    PUBLIC
        ${CMAKE_SOURCE_DIR}
)

wowdecomp_configure_windows_target(Base)
