include(${PROJECT_SOURCE_DIR}/cmake/NativeGxSources.cmake)
find_package(PkgConfig REQUIRED)
pkg_check_modules(DXVK REQUIRED IMPORTED_TARGET dxvk-d3d9)
add_library(WowNativeD3d9 STATIC
    ${WOWDECOMP_NATIVE_GX_SOURCES}
    ../Gx/CGxDeviceD3d/CGxDeviceD3d.cpp
    ../Gx/CGxDeviceD3d/Portable/Device.cpp
    ../Gx/Blit.cpp
    ../Tempest/C3Vector.cpp
    ../Tempest/CImVector.cpp)
target_compile_definitions(WowNativeD3d9 PUBLIC WOWDECOMP_NATIVE_D3D9=1)
target_link_libraries(WowNativeD3d9 PUBLIC WowNativeWindow PkgConfig::DXVK)
set(WOWDECOMP_NATIVE_GX_TARGET WowNativeD3d9)
include(${PROJECT_SOURCE_DIR}/cmake/NativeGxConsumers.cmake)
if(WOWDECOMP_CORE_SANITIZERS)
    target_compile_options(WowNativeD3d9 PRIVATE -g1)
    target_link_options(WowNativeD3d9 PUBLIC -shared-libasan)
endif()
