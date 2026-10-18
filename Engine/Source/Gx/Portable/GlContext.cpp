#include "GlContext.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <stdexcept>
#include <thread>

namespace Gx { namespace Native {
namespace {
std::runtime_error Failure(const std::string& action) {
    return std::runtime_error(action + ": " + SDL_GetError() +
        ". Require the Linux X11 desktop OpenGL 3.3 compatibility driver; core/ES is unsupported.");
}
int Attribute(SDL_GLAttr attribute) {
    int value = 0;
    if (!SDL_GL_GetAttribute(attribute, &value))
        throw Failure("Query OpenGL framebuffer/context attribute");
    return value;
}
}
struct GlContext::State {
    std::thread::id owner = std::this_thread::get_id();
    OsNative::Window window;
    SDL_GLContext context = nullptr;
    GlInfo info;
    State(const char* title, int width, int height, bool hidden)
        : window(title, width, height, hidden, OsNative::WindowGraphics::OpenGlCompatibility) {}
    void Check(bool current = false) const {
        if (std::this_thread::get_id() != owner)
            throw std::logic_error("Gx OpenGL operations require the process main thread");
        if (current && SDL_GL_GetCurrentContext() != context)
            throw std::logic_error("The owning Gx OpenGL context must be current");
    }
    ~State() {
        if (std::this_thread::get_id() != owner)
            std::terminate();
        if (context && !SDL_GL_DestroyContext(context))
            std::terminate();
        // window is released after its context, then SDL quits.
    }
};
GlContext::GlContext(const char* title, int width, int height, bool hidden)
    : state(std::make_unique<State>(title, width, height, hidden)) {
    state->context = SDL_GL_CreateContext(state->window.Handle());
    if (!state->context)
        throw Failure("Create SDL OpenGL compatibility context");
    MakeCurrent();
    auto& info = state->info;
    info.major = Attribute(SDL_GL_CONTEXT_MAJOR_VERSION);
    info.minor = Attribute(SDL_GL_CONTEXT_MINOR_VERSION);
    info.profile = Attribute(SDL_GL_CONTEXT_PROFILE_MASK);
    if (info.major < 3 || (info.major == 3 && info.minor < 3) ||
        info.profile != SDL_GL_CONTEXT_PROFILE_COMPATIBILITY)
        throw std::runtime_error("Gx requires desktop OpenGL >=3.3 compatibility; obtained a different profile/version");
    info.red = Attribute(SDL_GL_RED_SIZE);
    info.green = Attribute(SDL_GL_GREEN_SIZE);
    info.blue = Attribute(SDL_GL_BLUE_SIZE);
    info.alpha = Attribute(SDL_GL_ALPHA_SIZE);
    info.depth = Attribute(SDL_GL_DEPTH_SIZE);
    info.stencil = Attribute(SDL_GL_STENCIL_SIZE);
    info.doubleBuffered = Attribute(SDL_GL_DOUBLEBUFFER);
    if (info.red < 8 || info.green < 8 || info.blue < 8 || info.alpha < 8 ||
        info.depth < 24 || info.stencil < 8 || !info.doubleBuffered)
        throw std::runtime_error("Gx requires RGBA8, depth24, stencil8 and a double-buffered drawable");
    auto getString = reinterpret_cast<decltype(&glGetString)>(Proc("glGetString"));
    auto string = [&](GLenum name) {
        auto value = getString(name);
        if (!value)
            throw std::runtime_error("OpenGL returned a null identity string");
        return std::string(reinterpret_cast<const char*>(value));
    };
    info.vendor = string(GL_VENDOR);
    info.renderer = string(GL_RENDERER);
    info.version = string(GL_VERSION);
    info.video = state->window.VideoDriver();
    // Contracts traced from CGxDeviceOpenGl::CapsInitialize/GlExt.cpp. Native
    // render targets use FBOs; WGL pbuffers and NV/ATI optional paths are absent.
    for (const char* extension : {"GL_ARB_multitexture", "GL_ARB_vertex_program",
             "GL_ARB_fragment_program", "GL_ARB_vertex_buffer_object",
             "GL_EXT_texture_compression_s3tc", "GL_EXT_framebuffer_object"})
        if (!SDL_GL_ExtensionSupported(extension))
            throw std::runtime_error(std::string("Gx missing required extension ") + extension +
                "; use a desktop compatibility driver supporting original 12340 ARB programs/S3TC/FBOs");
    // An extension string alone is insufficient. Validate every required entry
    // point; function pointers are obtained only after this context is current.
    for (const char* name : {"glActiveTextureARB", "glClientActiveTextureARB",
             "glGenProgramsARB", "glBindProgramARB", "glProgramStringARB",
             "glDeleteProgramsARB", "glGetProgramivARB", "glIsProgramARB",
             "glProgramEnvParameter4fvARB", "glProgramLocalParameter4fvARB",
             "glVertexAttribPointerARB", "glEnableVertexAttribArrayARB", "glDisableVertexAttribArrayARB",
             "glBindBufferARB", "glGenBuffersARB", "glDeleteBuffersARB", "glBufferDataARB",
             "glBufferSubDataARB", "glMapBufferARB", "glUnmapBufferARB",
             "glCompressedTexImage2DARB", "glCompressedTexSubImage2DARB",
             "glGenFramebuffersEXT", "glBindFramebufferEXT", "glDeleteFramebuffersEXT",
             "glFramebufferTexture2DEXT", "glCheckFramebufferStatusEXT",
             "glGenRenderbuffersEXT", "glBindRenderbufferEXT", "glRenderbufferStorageEXT",
             "glFramebufferRenderbufferEXT", "glDeleteRenderbuffersEXT"})
        Proc(name);
    if (!SDL_GL_GetSwapInterval(&info.swapInterval))
        throw Failure("Query OpenGL swap interval");
}
GlContext::~GlContext() = default;
OsNative::Window& GlContext::Window() {
    state->Check();
    return state->window;
}
const GlInfo& GlContext::Info() const {
    state->Check();
    return state->info;
}
GlProc GlContext::Proc(const char* name) const {
    state->Check(true);
    auto proc = SDL_GL_GetProcAddress(name);
    if (!proc)
        throw Failure(std::string("Resolve required OpenGL procedure ") + name);
    return reinterpret_cast<GlProc>(proc);
}
void GlContext::MakeCurrent() {
    state->Check();
    if (!SDL_GL_MakeCurrent(state->window.Handle(), state->context))
        throw Failure("Make SDL OpenGL context current");
}
void GlContext::Present() {
    state->Check(true);
    if (!SDL_GL_SwapWindow(state->window.Handle()))
        throw Failure("Present SDL OpenGL drawable");
}
void GlContext::SwapInterval(int interval) {
    state->Check(true);
    if (!SDL_GL_SetSwapInterval(interval) || !SDL_GL_GetSwapInterval(&state->info.swapInterval))
        throw Failure("Set OpenGL swap interval");
    if (state->info.swapInterval != interval)
        throw std::runtime_error("Driver did not retain the requested OpenGL swap interval");
}
}}
