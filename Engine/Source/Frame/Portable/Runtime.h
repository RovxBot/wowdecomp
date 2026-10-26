#ifndef WOWDECOMP_NATIVE_LAYOUT_RUNTIME_H
#define WOWDECOMP_NATIVE_LAYOUT_RUNTIME_H
#include "Engine/Source/FrameXML/Portable/Runtime.h"
#include <cstdint>
#include <cstddef>
class CLayoutFrame;
class CFramePoint;
namespace Frame { namespace Native {
// Create inside XML/FrameScript owners. Close all frames and standalone points
// before this owner. Mutate public legacy fields only on this owner thread.
class Runtime final {
public:
    explicit Runtime(uint32_t frameLevelLimit = 128);
    ~Runtime();
    Runtime(const Runtime&) = delete;
    Runtime& operator=(const Runtime&) = delete;
private:
    FrameXML::Native::OwnerLease lifetime;
};
struct OwnerLease final {
    OwnerLease();
    ~OwnerLease();
    OwnerLease(const OwnerLease&) = delete;
    OwnerLease& operator=(const OwnerLease&) = delete;
};
void RequireOwner();
// Immutable for the lifetime of the existing layout owner. Parent depth and
// anchor/population bounds are separate; original level increments stay at 128.
uint32_t MaximumFrameLevel();
// Original batch callbacks may not mutate layout or derived Frame consumers.
void SetLayoutRenderActive(bool);
void RequireNoObjects();
void FrameCreated(CLayoutFrame*);
void FrameDestroyed(CLayoutFrame*);
void RequireFrame(CLayoutFrame*);
void RequirePoint(uint32_t);
void PointCreated(CFramePoint*);
void PointDestroyed(CFramePoint*);
void RequireAnchor(CLayoutFrame*, CLayoutFrame*);
void RequireFinite(float);
void RequireDimension(float);
void RequireDestroyable();
void RequireTreeMutable();
struct CallbackScope final {
    CallbackScope();
    ~CallbackScope();
    CallbackScope(const CallbackScope&) = delete;
    CallbackScope& operator=(const CallbackScope&) = delete;
};
// Original ordered Glue creation runs outside root traversal. Existing objects
// may change visibility/layout there. Actual resize/input/render guards apply.
struct LoadScriptEdits final {
    LoadScriptEdits();
    ~LoadScriptEdits();
    LoadScriptEdits(const LoadScriptEdits&) = delete;
    LoadScriptEdits& operator=(const LoadScriptEdits&) = delete;
};
// Private factory transaction capability: the checked Lua factory may construct
// and unwind new objects while its caller is pinned. Any callback invoked by the
// factory adds a pin and loses this capability until it returns. Never wrap
// arbitrary Lua execution or edits/deletion of existing objects in this scope.
struct FactoryScope final {
    FactoryScope();
    ~FactoryScope();
    FactoryScope(const FactoryScope&) = delete;
    FactoryScope& operator=(const FactoryScope&) = delete;
private:
    size_t previous;
};
struct ResizeScope final {
    ResizeScope();
    ~ResizeScope();
    ResizeScope(const ResizeScope&) = delete;
    ResizeScope& operator=(const ResizeScope&) = delete;
};
}}
#endif
