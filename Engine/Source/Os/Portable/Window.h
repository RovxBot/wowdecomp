#ifndef WOWDECOMP_NATIVE_WINDOW_H
#define WOWDECOMP_NATIVE_WINDOW_H
#include "Engine/Source/Os/Input.h"
#include <memory>
#include <string>
#include <vector>

struct SDL_Window;
namespace OsNative {
enum class WindowGraphics { Software, OpenGlCompatibility };
struct WindowState;
struct Composition {
    std::string text, result;
    int start = 0, length = 0;
    std::vector<std::string> candidates;
    int selected = -1;
};
class Window {
  public:
    Window(const char *title, int width, int height, bool hidden = false,
           WindowGraphics graphics = WindowGraphics::Software);
    ~Window();
    Window(const Window &) = delete;
    Window &operator=(const Window &) = delete;
    SDL_Window *Handle() const;
    void Pump();
    bool RelativeMouse(bool enabled);
    bool Fullscreen(bool enabled);
    bool IsRelative() const;
    void Size(int &width, int &height, int &pixelWidth, int &pixelHeight) const;
    void TextArea(int x, int y, int width, int height, int cursor);
    const Composition &CurrentComposition() const;
    const char *VideoDriver() const;

  private:
    std::unique_ptr<WindowState> state;
};
// Strict UTF-8 at the input boundary; no surrogate or out-of-range scalar values.
std::vector<uint32_t> DecodeUtf8(const std::string &text);
std::string EncodeUtf8(uint32_t scalar);
} // namespace OsNative
#endif
