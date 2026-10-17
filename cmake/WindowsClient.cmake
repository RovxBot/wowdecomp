# This configuration never enters a native core build.
if(NOT WOWDECOMP_OS STREQUAL "WINDOWS" OR NOT MSVC
   OR NOT WOWDECOMP_CPU MATCHES "^(X86|X86_64)$")
    message(FATAL_ERROR "The full client requires Windows x86/x64 and MSVC. Use a core preset for M1 checks.")
endif()
if(WOWDECOMP_CPU STREQUAL "X86_64")
    set(WOWDECOMP_WINDOWS_SOURCE_DIR Win64)
else()
    set(WOWDECOMP_WINDOWS_SOURCE_DIR Win32)
endif()
set(WOWDECOMP_PLATFORM_SOURCE_DIRS ${WOWDECOMP_WINDOWS_SOURCE_DIR} Windows)

add_library(wowdecomp_windows_options INTERFACE)
target_link_libraries(wowdecomp_windows_options INTERFACE wowdecomp_platform)
target_compile_definitions(wowdecomp_windows_options INTERFACE
    WIN32 _WINDOWS _CRT_SECURE_NO_WARNINGS
)
if(WOWDECOMP_ARCH_32)
    # Preserve x87 and time32 behavior for original Win32 layout comparisons.
    target_compile_options(wowdecomp_windows_options INTERFACE /arch:IA32 /fp:fast)
    target_compile_definitions(wowdecomp_windows_options INTERFACE _USE_32BIT_TIME_T)
endif()
if(WOWDECOMP_DISABLE_SIGNATURE_CHECKS)
    target_compile_definitions(wowdecomp_windows_options INTERFACE DISABLE_SIGNATURE_CHECKS=1)
endif()

function(wowdecomp_configure_windows_target target)
    target_link_libraries(${target} PRIVATE wowdecomp_windows_options)
    # Preserve the old SoundEngine directory's full Windows header surface.
    if(NOT target MATCHES "^(fmodex|SoundEngine)$")
        target_compile_definitions(${target} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    endif()
endfunction()
