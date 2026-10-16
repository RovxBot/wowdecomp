wowdecomp_collect_sources(PRIVATE_SOURCES)

add_library(ObjectMgrClient STATIC
    ${PRIVATE_SOURCES}
)

target_include_directories(ObjectMgrClient
    PUBLIC
        ${CMAKE_SOURCE_DIR}
)

wowdecomp_configure_windows_target(ObjectMgrClient)
