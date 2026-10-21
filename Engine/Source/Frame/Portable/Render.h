#ifndef WOWDECOMP_NATIVE_FRAME_RENDER_H
#define WOWDECOMP_NATIVE_FRAME_RENDER_H
#include "Engine/Source/Gxu/Portable/LegacyFonts.h"
class CRenderBatch;
namespace Frame { namespace Native {
// Owns original UI vertex shaders inside Gxu/device/context ownership. Close
// every batch first. Mesh submissions snapshot their arrays and retain resources;
// callback parameters and public legacy fields remain borrowed owner-thread state.
class Renderer final {
public:
    Renderer();
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    void CheckCanClose() const;
private:
    Gxu::Native::FontOwnerLease lifetime;
};
void RequireRenderer();
class RenderScope final {
public:
    explicit RenderScope(CRenderBatch*);
    ~RenderScope();
    RenderScope(const RenderScope&) = delete;
    RenderScope& operator=(const RenderScope&) = delete;
private:
    CRenderBatch* batch;
};
}}
#endif
