#ifndef WOWDECOMP_NATIVE_LAYOUT_RUNTIME_H
#define WOWDECOMP_NATIVE_LAYOUT_RUNTIME_H
#include "Engine/Source/FrameXML/Portable/Runtime.h"
#include <cstdint>
class CLayoutFrame;
class CFramePoint;
namespace Frame { namespace Native {
// Create inside XML/FrameScript owners. Close all frames and standalone points
// before this owner. Mutate public legacy fields only on this owner thread.
class Runtime final {
public:
    Runtime();
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
// may change visibility/layout there, while destruction and new Frame allocation
// remain blocked inside callbacks. Actual resize/input/render guards still apply.
struct LoadScriptEdits final {
    LoadScriptEdits();
    ~LoadScriptEdits();
    LoadScriptEdits(const LoadScriptEdits&) = delete;
    LoadScriptEdits& operator=(const LoadScriptEdits&) = delete;
};
struct ResizeScope final {
    ResizeScope();
    ~ResizeScope();
    ResizeScope(const ResizeScope&) = delete;
    ResizeScope& operator=(const ResizeScope&) = delete;
};
}}
#endif
