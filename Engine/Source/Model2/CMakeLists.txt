wowdecomp_collect_sources(PRIVATE_SOURCES)

add_library(Model2 STATIC
    ${PRIVATE_SOURCES}
)

target_include_directories(Model2
    PUBLIC
        ${CMAKE_SOURCE_DIR}
)

wowdecomp_configure_windows_target(Model2)
