#include "GlTransforms.h"
#include <SDL3/SDL_opengl.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace Gx { namespace Native {
namespace {
std::array<float, 16> Values(const C44Matrix& m) {
    return {{m.a0,m.a1,m.a2,m.a3, m.b0,m.b1,m.b2,m.b3,
             m.c0,m.c1,m.c2,m.c3, m.d0,m.d1,m.d2,m.d3}};
}
void Finite(const C44Matrix& matrix) {
    for (float value : Values(matrix))
        if (!std::isfinite(value))
            throw std::invalid_argument("Nonfinite Gx transform");
}
GlNormalizedRect Rect(GlNormalizedRect rect, bool emptyAllowed) {
    for (float value : {rect.minX, rect.maxX, rect.minY, rect.maxY})
        if (!std::isfinite(value))
            throw std::invalid_argument("Nonfinite Gx viewport/scissor");
    if (rect.minX > rect.maxX || rect.minY > rect.maxY)
        throw std::invalid_argument("Inverted Gx viewport/scissor");
    rect.minX = std::clamp(rect.minX, 0.0f, 1.0f);
    rect.maxX = std::clamp(rect.maxX, 0.0f, 1.0f);
    rect.minY = std::clamp(rect.minY, 0.0f, 1.0f);
    rect.maxY = std::clamp(rect.maxY, 0.0f, 1.0f);
    if (!emptyAllowed && (rect.minX == rect.maxX || rect.minY == rect.maxY))
        throw std::invalid_argument("Empty Gx viewport");
    return rect;
}
// Match legacy normalized edge rounding, with double intermediates to avoid
// float->int overflow even for a representable maximum GL extent.
std::array<GLint, 4> Pixels(const GlNormalizedRect& rect, int width, int height) {
    auto edge = [](float value, int extent) {
        return static_cast<GLint>(std::min(double(extent), double(value) * extent + 0.5));
    };
    GLint x = edge(rect.minX, width), y = edge(rect.minY, height);
    return {{x, y, edge(rect.maxX, width) - x, edge(rect.maxY, height) - y}};
}
}
struct GlTransforms::State {
    GlContext& context;
    GlRenderTargets& targets;
    std::array<CGxMatrixStack, GxXforms_Last> stacks;
    C44Matrix projection, nativeProjection;
    GlViewport viewport;
    GlNormalizedRect scissor;
    bool scissorEnabled = false, synced = false;
    uint32_t masterEnables = 0;
    GlTargetView syncedView;
#define BASE(name) decltype(&gl##name) name = reinterpret_cast<decltype(name)>(context.Proc("gl" #name))
    BASE(GetError);
    BASE(GetIntegerv);
    BASE(MatrixMode);
    BASE(LoadMatrixf);
    BASE(Viewport);
    BASE(DepthRange);
    BASE(Scissor);
    BASE(Enable);
    BASE(Disable);
    BASE(CullFace);
#undef BASE
    State(GlContext& owner, GlRenderTargets& targetOwner) : context(owner), targets(targetOwner) {}
    CGxMatrixStack& Stack(uint32_t which) {
        context.CheckCurrent();
        if (which >= stacks.size())
            throw std::out_of_range("Invalid Gx transform stack");
        return stacks[which];
    }
    void Check(const char* operation) const {
        GLenum error = GetError();
        if (error != GL_NO_ERROR)
            throw std::runtime_error(std::string(operation) + " GL error=" + std::to_string(error));
    }
};
GlTransforms::GlTransforms(GlContext& context, GlRenderTargets& targets) {
    context.CheckCurrent();
    state = std::make_unique<State>(context, targets);
    // Match IStateSetContextDefaults before the first matrix query. A fresh
    // shader-only native context has not selected any compatibility matrix.
    // In particular, the observed Radeon driver reports matrix mode zero until
    // this explicit selection; zero cannot be restored with glMatrixMode.
    state->MatrixMode(GL_MODELVIEW);
    state->Check("Initialize Gx compatibility matrix mode");
}
GlTransforms::~GlTransforms() = default;
void GlTransforms::Set(uint32_t which, const C44Matrix& matrix) {
    auto& stack = state->Stack(which);
    Finite(matrix);
    stack.m_mtx[stack.m_level] = matrix;
    stack.m_flags[stack.m_level] &= ~CGxMatrixStack::F_Identity;
    stack.m_dirty = 1;
    state->synced = false;
}
C44Matrix GlTransforms::Get(uint32_t which) const {
    auto& stack = state->Stack(which);
    return stack.m_mtx[stack.m_level];
}
void GlTransforms::Push(uint32_t which) {
    auto& stack = state->Stack(which);
    // Legacy pushes saturate at level three and copy level two again.
    stack.m_level = std::min(stack.m_level + 1, 3u);
    stack.m_mtx[stack.m_level] = stack.m_mtx[stack.m_level - 1];
    stack.m_flags[stack.m_level] = stack.m_flags[stack.m_level - 1];
    stack.m_dirty = 1;
    state->synced = false;
}
void GlTransforms::Pop(uint32_t which) {
    auto& stack = state->Stack(which);
    if (stack.m_level) --stack.m_level;
    stack.m_dirty = 1;
    state->synced = false;
}
void GlTransforms::Translate(uint32_t which, const C3Vector& move) {
    auto matrix = Get(which);
    matrix.Translate(move);
    Set(which, matrix);
}
void GlTransforms::Scale(uint32_t which, const C3Vector& scale) {
    auto matrix = Get(which);
    matrix.Scale(scale);
    Set(which, matrix);
}
void GlTransforms::Projection(const C44Matrix& matrix) {
    state->context.CheckCurrent();
    Finite(matrix);
    state->projection = matrix;
    state->synced = false;
}
C44Matrix GlTransforms::Projection() const {
    state->context.CheckCurrent();
    return state->projection;
}
void GlTransforms::Viewport(const GlViewport& viewport) {
    state->context.CheckCurrent();
    auto next = viewport;
    next.rect = Rect(next.rect, false);
    if (!std::isfinite(next.minZ) || !std::isfinite(next.maxZ) || next.minZ < 0 ||
        next.maxZ > 1 || next.minZ > next.maxZ)
        throw std::invalid_argument("Invalid Gx depth range");
    state->viewport = next;
    state->synced = false;
}
GlViewport GlTransforms::Viewport() const {
    state->context.CheckCurrent();
    return state->viewport;
}
void GlTransforms::Scissor(bool enabled, const GlNormalizedRect& rect) {
    state->context.CheckCurrent();
    auto next = Rect(rect, true);
    state->scissor = next;
    state->scissorEnabled = enabled;
    state->synced = false;
}
void GlTransforms::MasterEnables(uint32_t mask) {
    state->context.CheckCurrent();
    state->masterEnables = mask;
    state->synced = false;
}
void GlTransforms::Sync() {
    auto& s = *state;
    s.context.CheckCurrent();
    auto target = s.targets.View();
    if (target.width <= 0 || target.height <= 0)
        throw std::logic_error("Gx transforms require a nonempty bound target/drawable");
    s.Check("Before Gx transform synchronization");
    // 12340 IXformSetProjection and IXformGLModelView: row-vector Tempest
    // storage is consumed as column-major by GL. Do not transpose these loads.
    auto projection = s.projection;
    projection.c0 = -projection.c0; projection.c1 = -projection.c1;
    projection.c2 = -projection.c2; projection.c3 = -projection.c3;
    if (!(s.masterEnables & 0x80000000u) && s.projection.d3 != 1.0f) {
        C44Matrix compression(0.2f);
        projection = projection * compression;
    }
    if (target.offscreen) {
        projection.a1 = -projection.a1; projection.b1 = -projection.b1;
        projection.c1 = -projection.c1; projection.d1 = -projection.d1;
    }
    auto modelView = Get(GxXform_World) * Get(GxXform_View);
    modelView.a2 = -modelView.a2; modelView.b2 = -modelView.b2;
    modelView.c2 = -modelView.c2; modelView.d2 = -modelView.d2;
    Finite(projection);
    Finite(modelView);
    GLint previousMode = 0;
    s.GetIntegerv(GL_MATRIX_MODE, &previousMode);
    auto p = Values(projection), m = Values(modelView);
    auto viewport = Pixels(s.viewport.rect, target.width, target.height);
    auto scissor = Pixels(s.scissor, target.width, target.height);
    s.MatrixMode(GL_PROJECTION); s.LoadMatrixf(p.data());
    s.MatrixMode(GL_MODELVIEW); s.LoadMatrixf(m.data());
    s.MatrixMode(static_cast<GLenum>(previousMode));
    s.Viewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    s.DepthRange(s.viewport.minZ, s.viewport.maxZ);
    s.Scissor(scissor[0], scissor[1], scissor[2], scissor[3]);
    (s.scissorEnabled ? s.Enable : s.Disable)(GL_SCISSOR_TEST);
    s.CullFace(target.offscreen ? GL_FRONT : GL_BACK);
    s.Check("Synchronize native Gx transforms");
    s.nativeProjection = projection;
    s.syncedView = target;
    s.synced = true;
}
GlUiConstants GlTransforms::UiConstants() const {
    auto& s = *state;
    s.context.CheckCurrent();
    auto view = s.targets.View();
    if (!s.synced || view.width != s.syncedView.width || view.height != s.syncedView.height ||
        view.offscreen != s.syncedView.offscreen)
        throw std::logic_error("Synchronize Gx transforms before reading UI constants");
    auto projection = s.nativeProjection;
    projection.c0 = -projection.c0; projection.c1 = -projection.c1;
    projection.c2 = -projection.c2; projection.c3 = -projection.c3;
    auto matrix = (Get(GxXform_View) * projection).Transpose();
    Finite(matrix);
    auto values = Values(matrix);
    GlUiConstants result{};
    for (size_t row = 0; row < 4; ++row)
        std::copy_n(values.begin() + row * 4, 4, result[row].begin());
    return result;
}
}}
