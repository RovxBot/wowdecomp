#ifndef WOWDECOMP_NATIVE_GL_RENDER_TARGETS_H
#define WOWDECOMP_NATIVE_GL_RENDER_TARGETS_H
#include "Engine/Source/Gx/CGxDevice/Types.h"
#include "GlContext.h"
#include <cstdint>
#include <memory>

namespace Gx {
namespace Native {
enum class GlTargetFormat { Rgba8, Depth24 };
struct GlTargetSpec {
    EGxTexTarget target = GxTex_2d;
    uint32_t width = 0, height = 0;
    GlTargetFormat format = GlTargetFormat::Rgba8;
    bool implicitDepth = false;
};

// An immutable allocation, retained by every attachment/sampler using it. All
// handles and the target owner must die before their current GlContext. This is
// native storage, not a CGxTex layout or a legacy reference-count substitute.
class GlTarget {
  public:
    GlTarget() = default;
    explicit operator bool() const { return bool(storage); }
    GlTargetSpec Spec() const;

  private:
    struct Storage;
    std::shared_ptr<Storage> storage;
    friend class GlRenderTargets;
};
struct GlAttachment {
    GlTarget texture;
    uint32_t plane = 0; // Original Gx cube order: +X, -X, +Y, -Y, +Z, -Z.
};
struct GlTargetView {
    int width = 0, height = 0;
    bool offscreen = false;
};

// Native adaptation of the 12340 Gx FBO/texture ownership boundary. Attachment
// pairs commit together so an intermediate incomplete FBO is never published.
// It owns framebuffer and sampler bindings; callers must not change them behind
// it. GL operations/destruction require the current context on the main thread.
// Projection/state-cache integration remains the device adapter's responsibility.
class GlRenderTargets {
  public:
    explicit GlRenderTargets(GlContext &context);
    ~GlRenderTargets();
    GlRenderTargets(const GlRenderTargets &) = delete;
    GlRenderTargets &operator=(const GlRenderTargets &) = delete;
    GlTarget Create(const GlTargetSpec &spec);
    // Empty pair restores the drawable and refreshes its pixel-sized viewport.
    // Explicit depth overrides color's optional implicit depth renderbuffer.
    // Rejected transitions retain the previous attachments and viewport.
    void Bind(const GlAttachment &color = {}, const GlAttachment &depth = {});
    // Empty texture unbinds this owner's previous sampler at the selected unit.
    // Reading an attached texture, including another cube face, is rejected.
    void Sample(uint32_t unit, const GlTarget &texture = {});
    GlTargetView View() const;

  private:
    struct State;
    std::unique_ptr<State> state;
};
} // namespace Native
} // namespace Gx
#endif
