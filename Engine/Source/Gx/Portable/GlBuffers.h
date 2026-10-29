#ifndef WOWDECOMP_NATIVE_GL_BUFFERS_H
#define WOWDECOMP_NATIVE_GL_BUFFERS_H
#include "GlContext.h"
#include "Engine/Source/Gx/Types.h"
#include <cstddef>
#include <memory>

namespace Gx { namespace Native {
struct GlBufferSpec {
    // Raw enum indices are validated before interpretation, including negatives
    // supplied through an unsigned conversion. Index elements are uint16_t.
    uint32_t target = GxPoolTarget_Vertex, usage = GxPoolUsage_Static;
    uint32_t itemSize = 0, itemCount = 0;
};
class GlBuffer {
public:
    GlBuffer() = default;
    explicit operator bool() const { return bool(storage); }
    GlBufferSpec Spec() const;
private:
    struct Storage;
    std::shared_ptr<Storage> storage;
    friend class GlBuffers;
    friend class GlBufferWrite;
};

// A retained staging write, never a pointer into GPU memory. Dropping the token
// aborts without publishing bytes; the original allocation stays usable. All
// access/destruction requires the current owner context on the main thread.
class GlBufferWrite {
public:
    ~GlBufferWrite();
    GlBufferWrite(GlBufferWrite&&) noexcept;
    GlBufferWrite& operator=(GlBufferWrite&&) = delete;
    GlBufferWrite(const GlBufferWrite&) = delete;
    GlBufferWrite& operator=(const GlBufferWrite&) = delete;
    void* Data();
    uint32_t Size() const;
    // Zero means the entire allocation, matching the legacy BufUnlock caller
    // convention. A nonzero size publishes only that prefix. Commit ends the
    // write; its pointer then expires. Validation failures leave it retryable.
    void Commit(uint32_t size = 0);
private:
    struct State;
    std::unique_ptr<State> state;
    explicit GlBufferWrite(const GlBuffer& buffer);
    friend class GlBuffers;
};

struct GlVertexBinding {
    GlBuffer buffer;
    // Original Gx attribute indices (0..13) and type codes (0..6).
    uint32_t attribute = 0, type = 4, offset = 0, stride = 0;
};

// Native buffer/ARB vertex-input boundary; no CGxPool/CGxBuf layout, broker or
// global Gx export is substituted. Context outlives owner, handles and writes.
// Formats retain their buffers. A private VAO isolates array state; allocation,
// upload and draw restore the caller's VAO and array/element bindings. Disabled
// generic attributes retain caller-supplied constants; provide every shader
// input. Fixed submission supports position/normal/color and a selected UV set through the same
// checked storage and private VAO; GPU bone inputs are rejected.
class GlBuffers {
public:
    explicit GlBuffers(GlContext& context);
    ~GlBuffers();
    GlBuffers(const GlBuffers&) = delete;
    GlBuffers& operator=(const GlBuffers&) = delete;
    GlBuffer Create(const GlBufferSpec& spec);
    void Upload(const GlBuffer& buffer, const void* data, uint32_t size, uint32_t offset = 0);
    GlBufferWrite Lock(const GlBuffer& buffer);
    // Replace the complete format atomically; zero count clears it. Position
    // is mandatory for a draw. Zero stride means tightly packed elements.
    void VertexFormat(const GlVertexBinding* attributes, size_t count);
    void IndexBuffer(const GlBuffer& buffer = {});
    // Original Draw start/count semantics: vertices for arrays, uint16 elements
    // for indexed batches. No base-vertex reinterpretation. All fetched vertex
    // bytes and actual indices must have been uploaded and fit every binding.
    // Requires an enabled ARB vertex program unless fixedFunction is explicit.
    // Fixed draws require that program disabled. Zero count is a validated no-op.
    // Preflight a whole original batch before a sequence of internal passes.
    void ValidateDraw(uint32_t primitive, uint32_t start, uint32_t count, bool indexed = false,
                      bool fixedFunction = false) const;
    void Draw(uint32_t primitive, uint32_t start, uint32_t count, bool indexed = false,
              bool fixedFunction = false, uint32_t fixedTexCoord = GxVA_TexCoord0);
private:
    struct State;
    std::unique_ptr<State> state;
};
}}
#endif
