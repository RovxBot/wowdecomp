wowdecomp_collect_sources(PRIVATE_SOURCES)

if (NOT WIN32)
    list(FILTER PRIVATE_SOURCES EXCLUDE REGEX "/UILCD/Windows/")
endif ()

add_library(Ui STATIC
    ${PRIVATE_SOURCES}
)

target_include_directories(Ui
    PUBLIC
        ${CMAKE_SOURCE_DIR}
)

if (WIN32)
    target_link_libraries(Ui
        PRIVATE
            lgLcd
    )
endif ()

wowdecomp_configure_windows_target(Ui)
