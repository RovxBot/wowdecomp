#include "GlProgram.h"
#include <SDL3/SDL_opengl.h>
#include <SDL3/SDL_opengl_glext.h>
#include <stdexcept>

namespace Gx { namespace Native {
struct GlProgram::State {
    GlContext& context;
    GLenum target;
    GLuint name = 0;
    GLint constants = 0;
#define EXT(type, function) type function = reinterpret_cast<type>(context.Proc("gl" #function))
    EXT(PFNGLGENPROGRAMSARBPROC, GenProgramsARB);
    EXT(PFNGLBINDPROGRAMARBPROC, BindProgramARB);
    EXT(PFNGLPROGRAMSTRINGARBPROC, ProgramStringARB);
    EXT(PFNGLDELETEPROGRAMSARBPROC, DeleteProgramsARB);
    EXT(PFNGLGETPROGRAMIVARBPROC, GetProgramivARB);
    EXT(PFNGLPROGRAMENVPARAMETER4FVARBPROC, ProgramEnvParameter4fvARB);
#undef EXT
    decltype(&glGetError) GetError = reinterpret_cast<decltype(GetError)>(context.Proc("glGetError"));
    decltype(&glGetIntegerv) GetIntegerv = reinterpret_cast<decltype(GetIntegerv)>(context.Proc("glGetIntegerv"));
    decltype(&glGetString) GetString = reinterpret_cast<decltype(GetString)>(context.Proc("glGetString"));
    State(GlContext& owner, ShaderStage stage) : context(owner),
        target(stage == ShaderStage::Vertex ? GL_VERTEX_PROGRAM_ARB : GL_FRAGMENT_PROGRAM_ARB) {}
    ~State() {
        context.CheckCurrent();
        if (name) DeleteProgramsARB(1, &name);
    }
    void Check(const char* operation) const {
        GLenum error = GetError();
        if (error != GL_NO_ERROR)
            throw std::runtime_error(std::string(operation) + " GL error=" + std::to_string(error));
    }
};
GlProgram::GlProgram(GlContext& context, ShaderStage stage, const ShaderRecord& shader) {
    context.CheckCurrent();
    auto source = PrepareArbProgram(shader, stage);
    state = std::make_unique<State>(context, stage);
    auto& s = *state;
    s.Check("Before ARB program upload");
    GLint previous = 0;
    s.GetProgramivARB(s.target, GL_PROGRAM_BINDING_ARB, &previous);
    s.GetProgramivARB(s.target, GL_MAX_PROGRAM_ENV_PARAMETERS_ARB, &s.constants);
    s.GenProgramsARB(1, &s.name);
    s.BindProgramARB(s.target, s.name);
    s.ProgramStringARB(s.target, GL_PROGRAM_FORMAT_ASCII_ARB, static_cast<GLsizei>(source.size()), source.data());
    GLint position = -1, native = 0;
    s.GetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &position);
    s.GetProgramivARB(s.target, GL_PROGRAM_UNDER_NATIVE_LIMITS_ARB, &native);
    auto message = s.GetString(GL_PROGRAM_ERROR_STRING_ARB);
    std::string detail = message ? reinterpret_cast<const char*>(message) : "no driver diagnostic";
    // Restore even on a rejected upload; construction failure deletes its name.
    s.BindProgramARB(s.target, static_cast<GLuint>(previous));
    GLenum error = s.GetError();
    if (position != -1 || !native || error != GL_NO_ERROR)
        throw std::runtime_error("ARB program rejected at " + std::to_string(position) +
            " native=" + std::to_string(native) + " GL error=" + std::to_string(error) + ": " + detail);
}
GlProgram::~GlProgram() = default;
void GlProgram::Bind() const {
    state->context.CheckCurrent();
    state->BindProgramARB(state->target, state->name);
    state->Check("Bind ARB program");
}
void GlProgram::Constant(uint32_t index, const float (&value)[4]) const {
    state->context.CheckCurrent();
    if (index >= static_cast<uint32_t>(state->constants))
        throw std::out_of_range("ARB environment constant exceeds device limit");
    state->ProgramEnvParameter4fvARB(state->target, index, value);
    state->Check("Set ARB environment constant");
}
}}
