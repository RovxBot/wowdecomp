#ifndef WOWDECOMP_NATIVE_GL_CONTEXT_H
#define WOWDECOMP_NATIVE_GL_CONTEXT_H
#include "Engine/Source/Os/Portable/Window.h"
#include <memory>
#include <string>

// Native ABI host boundary for the 12340 compatibility renderer. It deliberately
// exposes neither WGL handles nor a legacy CGxDevice layout/address claim.
namespace Gx { namespace Native {
struct GlInfo {
    std::string vendor, renderer, version, video;
    int major = 0, minor = 0, profile = 0;
    int red = 0, green = 0, blue = 0, alpha = 0, depth = 0, stencil = 0;
    int doubleBuffered = 0, swapInterval = 0;
};
using GlProc = void (*)();
class GlContext {
public:
    GlContext(const char* title, int width, int height, bool hidden = false);
    ~GlContext();
    GlContext(const GlContext&) = delete;
    GlContext& operator=(const GlContext&) = delete;
    OsNative::Window& Window();
    const GlInfo& Info() const;
    // All methods, procedure calls and destruction belong to the process main
    // thread. Procedures belong to this context and expire before its window.
    GlProc Proc(const char* name) const;
    void MakeCurrent();
    void Present();
    // A rejected swap interval is an explicit failure; no silent fallback.
    void SwapInterval(int interval);
private:
    struct State;
    std::unique_ptr<State> state;
};
}}
#endif
