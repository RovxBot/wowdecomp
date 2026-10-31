// Check every dynamically imported DXVK SDL3 WSI prototype against the pinned
// SDL declarations, including return qualifiers and the calling convention.
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <type_traits>
#define SDL_PROC(ret, name, params) \
    static_assert(std::is_same_v<ret (SDLCALL*) params, decltype(&::name)>, "DXVK SDL3 ABI mismatch: " #name);
#include "wsi_platform_sdl3_funcs.h"
