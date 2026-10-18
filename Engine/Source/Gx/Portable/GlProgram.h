#ifndef WOWDECOMP_NATIVE_GL_PROGRAM_H
#define WOWDECOMP_NATIVE_GL_PROGRAM_H
#include "GlContext.h"
#include "ShaderPackage.h"

namespace Gx { namespace Native {
// Context-owned native GPU resource. The context must outlive this object and
// remain current for construction, use and destruction on its main thread.
class GlProgram {
public:
    GlProgram(GlContext& context, ShaderStage stage, const ShaderRecord& shader);
    ~GlProgram();
    GlProgram(const GlProgram&) = delete;
    GlProgram& operator=(const GlProgram&) = delete;
    void Bind() const;
    // Existing Gx constant convention: one index addresses one float4.
    void Constant(uint32_t index, const float (&value)[4]) const;
private:
    struct State;
    std::unique_ptr<State> state;
};
}}
#endif
