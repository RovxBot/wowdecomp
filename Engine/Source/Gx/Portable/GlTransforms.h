#ifndef WOWDECOMP_NATIVE_GL_TRANSFORMS_H
#define WOWDECOMP_NATIVE_GL_TRANSFORMS_H
#include "Engine/Source/Gx/CGxMatrixStack.h"
#include "Engine/Source/Gx/Types.h"
#include "GlRenderTargets.h"
#include <array>

namespace Gx { namespace Native {
struct GlNormalizedRect {
    float minX = 0, maxX = 1, minY = 0, maxY = 1;
};
struct GlViewport {
    GlNormalizedRect rect;
    float minZ = 0, maxZ = 1;
};
using GlUiConstants = std::array<std::array<float, 4>, 4>;

// Native adaptation of the 12340 Gx transform boundary, using the existing
// scalar Tempest matrices and four-entry CGxMatrixStack. This is not a
// CGxDevice layout, state cache or a replacement for legacy global Gx exports.
// Context and target owner must outlive this object. All access is owner-thread,
// current-context only. Construction selects the legacy GL_MODELVIEW baseline;
// Sync preserves the caller's subsequent matrix-mode selection. Call Sync after
// target changes and before drawing. Native offscreen orientation uses the
// original default capability branch (unk_00B0 == 0).
class GlTransforms {
public:
    GlTransforms(GlContext& context, GlRenderTargets& targets);
    ~GlTransforms();
    GlTransforms(const GlTransforms&) = delete;
    GlTransforms& operator=(const GlTransforms&) = delete;
    // Accept enum constants as unsigned indices; reject invalid raw values
    // before interpreting them as a C++ enum.
    void Set(uint32_t which, const C44Matrix& matrix);
    C44Matrix Get(uint32_t which) const;
    void Push(uint32_t which);
    void Pop(uint32_t which);
    void Translate(uint32_t which, const C3Vector& move);
    void Scale(uint32_t which, const C3Vector& scale);
    // As in CGxDevice, the projection setter is distinct from the stack slot.
    void Projection(const C44Matrix& matrix);
    C44Matrix Projection() const;
    void Viewport(const GlViewport& viewport);
    GlViewport Viewport() const;
    // Scissor coordinates use the same lower-left normalized target space as
    // legacy Gx; empty/outside rectangles clip everything. Sync owns its enable.
    void Scissor(bool enabled, const GlNormalizedRect& rect = {});
    // Preserve the inspected legacy projection branch literally: bit 31
    // suppresses perspective *0.2 compression (not the named bit-7 flag).
    // This does not establish that the historical warped-scene bug is fixed.
    void MasterEnables(uint32_t mask);
    void Sync();
    // Original GxXformViewProjNativeTranspose contract; excludes world, and
    // undoes the native projection's Z row conversion before transposition.
    // Call Sync first, including after a target/projection/view change.
    GlUiConstants UiConstants() const;
private:
    struct State;
    std::unique_ptr<State> state;
};
}}
#endif
