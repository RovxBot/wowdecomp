#include "GlBuffers.h"
#include <SDL3/SDL_opengl.h>
#include <SDL3/SDL_opengl_glext.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace Gx { namespace Native {
namespace {
struct Api {
    GlContext& context;
#define BASE(name) decltype(&gl##name) name = reinterpret_cast<decltype(name)>(context.Proc("gl" #name))
    BASE(GetError); BASE(GetIntegerv); BASE(IsEnabled); BASE(DrawArrays); BASE(DrawElements);
    BASE(EnableClientState); BASE(DisableClientState); BASE(VertexPointer);
    BASE(NormalPointer); BASE(ColorPointer); BASE(TexCoordPointer);
#undef BASE
#define EXT(type, name) type name = reinterpret_cast<type>(context.Proc("gl" #name))
    EXT(PFNGLCLIENTACTIVETEXTUREARBPROC, ClientActiveTextureARB);
    EXT(PFNGLGENBUFFERSARBPROC, GenBuffersARB);
    EXT(PFNGLDELETEBUFFERSARBPROC, DeleteBuffersARB);
    EXT(PFNGLBINDBUFFERARBPROC, BindBufferARB);
    EXT(PFNGLBUFFERDATAARBPROC, BufferDataARB);
    EXT(PFNGLBUFFERSUBDATAARBPROC, BufferSubDataARB);
    EXT(PFNGLGENVERTEXARRAYSPROC, GenVertexArrays);
    EXT(PFNGLDELETEVERTEXARRAYSPROC, DeleteVertexArrays);
    EXT(PFNGLBINDVERTEXARRAYPROC, BindVertexArray);
    EXT(PFNGLVERTEXATTRIBPOINTERARBPROC, VertexAttribPointerARB);
    EXT(PFNGLENABLEVERTEXATTRIBARRAYARBPROC, EnableVertexAttribArrayARB);
    EXT(PFNGLDISABLEVERTEXATTRIBARRAYARBPROC, DisableVertexAttribArrayARB);
#undef EXT
    explicit Api(GlContext& owner) : context(owner) {}
    GLint Get(GLenum name) const { GLint value = 0; GetIntegerv(name, &value); return value; }
    void Check(const char* operation) const {
        context.CheckCurrent();
        auto error = GetError();
        if (error != GL_NO_ERROR)
            throw std::runtime_error(std::string(operation) + " GL error=" + std::to_string(error));
    }
};
struct Bindings {
    Api& api;
    GLint array, vao, clientTexture;
    explicit Bindings(Api& value) : api(value), array(api.Get(GL_ARRAY_BUFFER_BINDING)),
        vao(api.Get(GL_VERTEX_ARRAY_BINDING)), clientTexture(api.Get(GL_CLIENT_ACTIVE_TEXTURE_ARB)) {}
    ~Bindings() {
        api.BindVertexArray(static_cast<GLuint>(vao));
        api.BindBufferARB(GL_ARRAY_BUFFER_ARB, static_cast<GLuint>(array));
        api.ClientActiveTextureARB(static_cast<GLenum>(clientTexture));
    }
};
// Same seven type codes as 12340 s_glAttribSize/Type/Normalized. Packed color
// bytes are passed in their original order; this layer performs no BGRA swizzle.
struct Type { GLint count; GLenum kind; GLboolean normalized; uint32_t size; };
constexpr Type types[] = {{4, GL_UNSIGNED_BYTE, GL_TRUE, 4}, {4, GL_UNSIGNED_BYTE, GL_FALSE, 4},
    {4, GL_UNSIGNED_BYTE, GL_TRUE, 4}, {2, GL_FLOAT, GL_TRUE, 8},
    {3, GL_FLOAT, GL_TRUE, 12}, {2, GL_SHORT, GL_TRUE, 4}, {1, GL_FLOAT, GL_TRUE, 4}};
constexpr GLenum primitives[] = {GL_POINTS, GL_LINES, GL_LINE_STRIP, GL_TRIANGLES,
    GL_TRIANGLE_STRIP, GL_TRIANGLE_FAN};
constexpr GLenum usages[] = {GL_STATIC_DRAW_ARB, GL_DYNAMIC_DRAW_ARB, GL_STREAM_DRAW_ARB};
using Range = std::pair<uint32_t, uint32_t>;
std::vector<Range> Covered(const std::vector<Range>& previous, uint32_t start, uint32_t end) {
    auto ranges = previous;
    ranges.emplace_back(start, end);
    std::sort(ranges.begin(), ranges.end());
    size_t count = 0;
    for (const auto range : ranges) {
        if (count && range.first <= ranges[count - 1].second)
            ranges[count - 1].second = std::max(ranges[count - 1].second, range.second);
        else ranges[count++] = range;
    }
    ranges.resize(count);
    return ranges;
}
}

struct GlBuffer::Storage {
    std::shared_ptr<Api> api;
    GlBufferSpec spec;
    GLuint name = 0;
    bool locked = false;
    std::vector<uint8_t> shadow;
    std::vector<Range> initialized;
    Storage(std::shared_ptr<Api> owner, const GlBufferSpec& value, uint32_t size)
        : api(std::move(owner)), spec(value), shadow(size) {}
    ~Storage() {
        api->context.CheckCurrent();
        if (name) api->DeleteBuffersARB(1, &name);
    }
    void RequireBytes(uint64_t start, uint64_t size) const {
        if (start > shadow.size() || size > shadow.size() - start)
            throw std::out_of_range("Gx draw fetch exceeds buffer storage");
        if (!size) return;
        auto found = std::upper_bound(initialized.begin(), initialized.end(), start,
            [](uint64_t value, const Range& range) { return value < range.first; });
        if (found == initialized.begin() || (--found)->second < start + size)
            throw std::logic_error("Gx draw fetches bytes that have not been uploaded");
    }
    void Publish(const void* data, uint32_t size, uint32_t offset) {
        api->context.CheckCurrent();
        api->Check("Before native buffer upload");
        if (offset > shadow.size() || size > shadow.size() - offset)
            throw std::out_of_range("Native buffer upload exceeds storage");
        if (size && !data) throw std::invalid_argument("Null native buffer upload");
        if (!size) return;
        // All CPU allocations/validation precede GPU mutation.
        auto coverage = Covered(initialized, offset, offset + size);
        Bindings restore(*api);
        // Buffer storage is target-independent. Use ARRAY_BUFFER for updates so
        // an element upload cannot mutate the caller's VAO element binding.
        api->BindBufferARB(GL_ARRAY_BUFFER_ARB, name);
        api->BufferSubDataARB(GL_ARRAY_BUFFER_ARB, offset, size, data);
        api->Check("Upload native buffer bytes");
        std::memcpy(shadow.data() + offset, data, size);
        initialized = std::move(coverage);
    }
};
GlBufferSpec GlBuffer::Spec() const {
    if (!storage) throw std::logic_error("Empty Gx buffer handle");
    return storage->spec;
}
struct GlBufferWrite::State {
    std::shared_ptr<GlBuffer::Storage> buffer;
    std::vector<uint8_t> bytes;
    explicit State(const GlBuffer& value) : buffer(value.storage), bytes(buffer->shadow) {
        buffer->locked = true;
    }
    ~State() {
        buffer->api->context.CheckCurrent();
        buffer->locked = false;
    }
};
GlBufferWrite::GlBufferWrite(const GlBuffer& buffer) : state(std::make_unique<State>(buffer)) {}
GlBufferWrite::GlBufferWrite(GlBufferWrite&&) noexcept = default;
GlBufferWrite::~GlBufferWrite() = default;
void* GlBufferWrite::Data() {
    if (!state) throw std::logic_error("Expired Gx buffer write");
    state->buffer->api->context.CheckCurrent();
    return state->bytes.data();
}
uint32_t GlBufferWrite::Size() const {
    if (!state) throw std::logic_error("Expired Gx buffer write");
    state->buffer->api->context.CheckCurrent();
    return static_cast<uint32_t>(state->bytes.size());
}
void GlBufferWrite::Commit(uint32_t size) {
    if (!state) throw std::logic_error("Expired Gx buffer write");
    state->buffer->api->context.CheckCurrent();
    if (size > state->bytes.size()) throw std::out_of_range("Gx unlock exceeds write capacity");
    state->buffer->Publish(state->bytes.data(), size ? size : Size(), 0);
    state.reset();
}

struct GlBuffers::State {
    std::shared_ptr<Api> api;
    GLuint vao = 0;
    std::vector<GlVertexBinding> attributes;
    GlBuffer index;
    explicit State(GlContext& owner) : api(std::make_shared<Api>(owner)) {
        api->Check("Before native buffer initialization");
        if (api->Get(GL_MAX_VERTEX_ATTRIBS_ARB) < 14)
            throw std::runtime_error("Gx requires fourteen ARB vertex attributes");
        api->GenVertexArrays(1, &vao);
        // State's destructor also handles failed owner construction.
    }
    ~State() {
        api->context.CheckCurrent();
        if (vao) api->DeleteVertexArrays(1, &vao);
    }
    void Validate(const GlBuffer& buffer, uint32_t target) const {
        api->context.CheckCurrent();
        if (!buffer || buffer.storage->api != api || buffer.Spec().target != target)
            throw std::invalid_argument("Gx buffer belongs to another owner or target");
    }
};
GlBuffers::GlBuffers(GlContext& context) {
    context.CheckCurrent();
    state = std::make_unique<State>(context);
    state->api->Check("Allocate native vertex array");
    if (!state->vao) throw std::runtime_error("OpenGL did not allocate a vertex array");
}
GlBuffers::~GlBuffers() = default;
GlBuffer GlBuffers::Create(const GlBufferSpec& spec) {
    auto& a = *state->api;
    a.context.CheckCurrent();
    if (spec.target >= GxPoolTargets_Last || spec.usage >= GxPoolUsages_Last ||
        !spec.itemSize || !spec.itemCount || (spec.target == GxPoolTarget_Index && spec.itemSize != 2))
        throw std::invalid_argument("Invalid native Gx buffer specification");
    uint64_t size = uint64_t(spec.itemSize) * spec.itemCount;
    if (size > UINT32_MAX || size > uint64_t(std::numeric_limits<GLsizeiptrARB>::max()))
        throw std::out_of_range("Gx buffer size exceeds its addressing contract");
    a.Check("Before native buffer allocation");
    auto storage = std::make_shared<GlBuffer::Storage>(state->api, spec, static_cast<uint32_t>(size));
    {
        Bindings restore(a);
        a.GenBuffersARB(1, &storage->name);
        a.BindBufferARB(GL_ARRAY_BUFFER_ARB, storage->name);
        a.BufferDataARB(GL_ARRAY_BUFFER_ARB, static_cast<GLsizeiptrARB>(size), nullptr, usages[spec.usage]);
    }
    a.Check("Allocate native buffer storage");
    if (!storage->name) throw std::runtime_error("OpenGL did not allocate a buffer name");
    GlBuffer result;
    result.storage = std::move(storage);
    return result;
}
void GlBuffers::Upload(const GlBuffer& buffer, const void* data, uint32_t size, uint32_t offset) {
    // Validate owner without trusting a potentially foreign handle's target.
    state->api->context.CheckCurrent();
    if (!buffer || buffer.storage->api != state->api)
        throw std::invalid_argument("Gx upload buffer belongs to another owner");
    if (buffer.storage->locked) throw std::logic_error("Cannot upload a locked Gx buffer");
    buffer.storage->Publish(data, size, offset);
}
GlBufferWrite GlBuffers::Lock(const GlBuffer& buffer) {
    state->api->context.CheckCurrent();
    if (!buffer || buffer.storage->api != state->api)
        throw std::invalid_argument("Gx write buffer belongs to another owner");
    if (buffer.storage->locked) throw std::logic_error("Gx buffer already locked");
    return GlBufferWrite(buffer);
}
void GlBuffers::VertexFormat(const GlVertexBinding* attributes, size_t count) {
    state->api->context.CheckCurrent();
    if (count > 14 || (count && !attributes)) throw std::invalid_argument("Invalid Gx vertex format");
    uint32_t mask = 0;
    for (size_t i = 0; i < count; ++i) {
        const auto& field = attributes[i];
        state->Validate(field.buffer, GxPoolTarget_Vertex);
        if (field.attribute >= 14 || field.type >= std::size(types) || (mask & (1u << field.attribute)))
            throw std::invalid_argument("Invalid/duplicate Gx vertex attribute or type");
        const auto& type = types[field.type];
        if (field.stride > uint32_t(std::numeric_limits<GLsizei>::max()) ||
            (field.stride && field.stride < type.size) ||
            field.offset > field.buffer.storage->shadow.size() ||
            type.size > field.buffer.storage->shadow.size() - field.offset)
            throw std::out_of_range("Gx vertex attribute exceeds its storage/stride");
        mask |= 1u << field.attribute;
    }
    std::vector<GlVertexBinding> next;
    if (count) next.assign(attributes, attributes + count);
    state->attributes = std::move(next);
}
void GlBuffers::IndexBuffer(const GlBuffer& buffer) {
    state->api->context.CheckCurrent();
    if (buffer) state->Validate(buffer, GxPoolTarget_Index);
    state->index = buffer;
}
void GlBuffers::ValidateDraw(uint32_t primitive, uint32_t start, uint32_t count, bool indexed, bool fixedFunction) const {
    auto& s = *state;
    auto& a = *s.api;
    a.context.CheckCurrent();
    if (primitive >= std::size(primitives) || count > uint32_t(std::numeric_limits<GLsizei>::max()))
        throw std::out_of_range("Invalid Gx primitive/count");
    if (!count) return;
    auto position = std::find_if(s.attributes.begin(), s.attributes.end(),
        [](const GlVertexBinding& field) { return field.attribute == GxVA_Position; });
    if (position == s.attributes.end()) throw std::logic_error("Gx draw requires a position binding");
    for (const auto& field : s.attributes)
        if (field.buffer.storage->locked) throw std::logic_error("Gx draw uses a locked vertex buffer");
    if (indexed) {
        s.Validate(s.index, GxPoolTarget_Index);
        if (s.index.storage->locked) throw std::logic_error("Gx draw uses a locked index buffer");
        s.index.storage->RequireBytes(uint64_t(start) * 2, uint64_t(count) * 2);
    } else if (start > uint32_t(std::numeric_limits<GLint>::max())) {
        throw std::out_of_range("Gx first vertex exceeds GL addressing");
    }
    // Validate every actual fetch, rather than trusting legacy min/max hints or
    // assuming sparse attribute/index uploads initialized the intervening bytes.
    for (uint32_t i = 0; i < count; ++i) {
        uint64_t vertex = uint64_t(start) + i;
        if (indexed) {
            uint16_t index;
            std::memcpy(&index, s.index.storage->shadow.data() + (uint64_t(start) + i) * 2, 2);
            vertex = index;
        }
        for (const auto& field : s.attributes) {
            uint32_t size = types[field.type].size;
            field.buffer.storage->RequireBytes(uint64_t(field.offset) +
                vertex * (field.stride ? field.stride : size), size);
        }
    }
    a.Check("Before native buffered draw");
    if (bool(a.IsEnabled(GL_VERTEX_PROGRAM_ARB)) == fixedFunction)
        throw std::logic_error("Native buffered draw vertex pipeline mismatch");
    if (fixedFunction) {
        for (const auto& field : s.attributes) {
            const bool vector = (field.attribute == GxVA_Position || field.attribute == GxVA_Normal) && field.type == 4;
            const bool color = field.attribute == GxVA_Color0 && (field.type == 0 || field.type == 2);
            const bool uv = (field.attribute == GxVA_TexCoord0 || field.attribute == GxVA_TexCoord1) && field.type == 3;
            if (!vector && !color && !uv)
                throw std::invalid_argument("Unsupported native fixed vertex input");
        }
    }
}
void GlBuffers::Draw(uint32_t primitive, uint32_t start, uint32_t count, bool indexed, bool fixedFunction,
                     uint32_t fixedTexCoord) {
    ValidateDraw(primitive, start, count, indexed, fixedFunction);
    if (fixedTexCoord != GxVA_TexCoord0 && fixedTexCoord != GxVA_TexCoord1)
        throw std::invalid_argument("Unsupported native fixed texture-coordinate selection");
    if (!count) return;
    auto& s = *state;
    auto& a = *s.api;
    {
        Bindings restore(a);
        a.BindVertexArray(s.vao);
        for (GLuint i = 0; i < 14; ++i) a.DisableVertexAttribArrayARB(i);
        a.ClientActiveTextureARB(GL_TEXTURE0_ARB);
        for (GLenum array : {GL_VERTEX_ARRAY, GL_NORMAL_ARRAY, GL_COLOR_ARRAY, GL_TEXTURE_COORD_ARRAY})
            a.DisableClientState(array);
        for (const auto& field : s.attributes) {
            const auto& type = types[field.type];
            a.BindBufferARB(GL_ARRAY_BUFFER_ARB, field.buffer.storage->name);
            const void* offset = reinterpret_cast<const void*>(uintptr_t(field.offset));
            if (!fixedFunction) {
                a.VertexAttribPointerARB(field.attribute, type.count, type.kind, type.normalized, field.stride, offset);
                a.EnableVertexAttribArrayARB(field.attribute);
            } else if (field.attribute == GxVA_Position) {
                a.VertexPointer(type.count, type.kind, field.stride, offset);
                a.EnableClientState(GL_VERTEX_ARRAY);
            } else if (field.attribute == GxVA_Normal) {
                a.NormalPointer(type.kind, field.stride, offset);
                a.EnableClientState(GL_NORMAL_ARRAY);
            } else if (field.attribute == GxVA_Color0) {
                a.ColorPointer(type.count, type.kind, field.stride, offset);
                a.EnableClientState(GL_COLOR_ARRAY);
            } else if (field.attribute == fixedTexCoord) {
                a.TexCoordPointer(type.count, type.kind, field.stride, offset);
                a.EnableClientState(GL_TEXTURE_COORD_ARRAY);
            }
            // Composition selects either original UV set on the same unit zero.
        }
        if (indexed) {
            a.BindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, s.index.storage->name);
            a.DrawElements(primitives[primitive], count, GL_UNSIGNED_SHORT,
                reinterpret_cast<const void*>(uintptr_t(start) * 2));
        } else a.DrawArrays(primitives[primitive], static_cast<GLint>(start), count);
    }
    a.Check("Submit native buffered draw");
}
}}
