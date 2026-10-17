// SDL-only control for desktop lifetime diagnostics. No engine or game data.
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <charconv>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string_view>

int Number(const char *text, int maximum) {
    std::string_view value(text);
    int result = 0;
    auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() ||
        result < 1 || result > maximum)
        throw std::invalid_argument("Invalid control count/duration");
    return result;
}

void Cycle(int index, int milliseconds, bool software) {
    if (!SDL_Init(SDL_INIT_VIDEO))
        throw std::runtime_error(SDL_GetError());
    struct Quit {
        ~Quit() { SDL_Quit(); }
    } quit;
    std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)> window(
        SDL_CreateWindow("M2 SDL-only desktop control", 640, 480, 0), SDL_DestroyWindow);
    if (!window)
        throw std::runtime_error(SDL_GetError());
    std::unique_ptr<SDL_Renderer, decltype(&SDL_DestroyRenderer)> renderer(nullptr, SDL_DestroyRenderer);
    if (software) {
        renderer.reset(SDL_CreateRenderer(window.get(), "software"));
        if (!renderer)
            throw std::runtime_error(SDL_GetError());
    }
    if (!SDL_StartTextInput(window.get()))
        throw std::runtime_error(SDL_GetError());
    std::cout << "cycle-ready=" << index << " video=" << SDL_GetCurrentVideoDriver()
              << " renderer=" << (software ? SDL_GetRendererName(renderer.get()) : "none") << std::endl;
    auto started = SDL_GetTicks();
    bool close = false;
    while (!close && SDL_GetTicks() - started < static_cast<Uint64>(milliseconds)) {
        SDL_Event event;
        while (SDL_PollEvent(&event))
            close |= event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED;
        if (renderer) {
            if (!SDL_SetRenderDrawColor(renderer.get(), 24, 40, 64, 255) ||
                !SDL_RenderClear(renderer.get()) || !SDL_RenderPresent(renderer.get()))
                throw std::runtime_error(SDL_GetError());
        }
        SDL_Delay(10);
    }
    if (!SDL_StopTextInput(window.get()))
        throw std::runtime_error(SDL_GetError());
    // Renderer and window destruction precede SDL_Quit, including error paths.
}

int main(int argc, char **argv) {
    try {
        int cycles = 1, milliseconds = 250;
        bool software = false;
        for (int i = 1; i < argc; ++i) {
            std::string_view argument(argv[i]);
            if (argument == "--software")
                software = true;
            else if (argument == "--cycles" && i + 1 < argc)
                cycles = Number(argv[++i], 16);
            else if (argument == "--frame-ms" && i + 1 < argc)
                milliseconds = Number(argv[++i], 10000);
            else
                throw std::invalid_argument("Usage: SDLDesktopControl [--software] [--cycles 1..16] [--frame-ms 1..10000]");
        }
        SDL_SetMainReady();
        for (int i = 1; i <= cycles; ++i) {
            Cycle(i, milliseconds, software);
            std::cout << "cycle-closed=" << i << std::endl;
        }
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << std::endl;
        return 1;
    }
}
