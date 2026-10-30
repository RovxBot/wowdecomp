#ifndef WOWDECOMP_NATIVE_FRAME_HIERARCHY_H
#define WOWDECOMP_NATIVE_FRAME_HIERARCHY_H
#include "Regions.h"
class CSimpleTop;
class CSimpleTitleRegion;
class CSimpleTexture;
class CSimpleScrollFrame;
namespace Frame { namespace Native {
// Frames are owned by their parent/root. Heap and selected pool allocations may
// coexist. Borrow until destruction; XML/Lua factory registration is separate.
void RequireRoot();
using ScrollNotify = void (*)(CSimpleFrame*);
void SetScrollNotify(ScrollNotify);
bool HasScrollSupport();
void NotifyScroll(CSimpleFrame*);
void RequireScriptTextMutation();
void RequireVisibilityMutation(CSimpleFrame*, bool showing);
// Only the original scroll-range notification may update qualified existing
// controls while layer traversal is pinned. No tree/layout/deletion capability.
struct ScrollRangeEdits final {
    ScrollRangeEdits();
    ~ScrollRangeEdits();
    ScrollRangeEdits(const ScrollRangeEdits&) = delete;
    ScrollRangeEdits& operator=(const ScrollRangeEdits&) = delete;
};
// Enter only around the original leaf wheel script during guarded dispatch.
// Nested Slider/scroll/visibility/Button callbacks share the bounded state
// admission; hierarchy, layout, input reentry and deletion stay pinned.
class MouseWheelEdits final {
public:
    ~MouseWheelEdits();
    MouseWheelEdits(const MouseWheelEdits&) = delete;
    MouseWheelEdits& operator=(const MouseWheelEdits&) = delete;
private:
    friend class ::CSimpleFrame;
    MouseWheelEdits();
    bool active = false;
};
CSimpleFontString* CreateFontString(CSimpleFrame*, uint32_t layer, bool show);
CSimpleTexture* CreateTexture(CSimpleFrame*, uint32_t layer, bool show);
CSimpleTitleRegion* CreateTitleRegion(CSimpleFrame*);
void SimpleFrameCreated(CSimpleFrame*);
void SimpleFrameDestroyed(CSimpleFrame*);
void RequireScale(CSimpleFrame*, float);
void RequireLevel(CSimpleFrame*, int32_t);
void RequireRaise(CSimpleFrame*);
[[noreturn]] void UnavailableFrameFeature();
// Deferral is separate from CSimpleTop::m_destroyed's attribute-permission list.
// Call on the owner outside batch rendering; it is allowed during layer updates.
void DestroyLater(CSimpleFrame*);
void DrainDestroyQueue();
void ClearDirtyBatches(CSimpleTop&);
void PrepareScrollBatches();
void DrawScrollChild(CSimpleFrame*);
// Only the original ScrollFrame callback may enter this traversal. All layout,
// tree, batch mutation and general nested draw guards remain active inside it.
class ScrollRenderScope final {
public:
    ~ScrollRenderScope();
    ScrollRenderScope(const ScrollRenderScope&) = delete;
    ScrollRenderScope& operator=(const ScrollRenderScope&) = delete;
private:
    friend class ::CSimpleScrollFrame;
    friend void DrawScrollChild(CSimpleFrame*);
    ScrollRenderScope(CSimpleFrame*, CSimpleFrame*, void (*)(void*));
    void Draw(CSimpleFrame*, size_t depth);
    ScrollRenderScope* previous;
    CSimpleFrame* parent;
    CSimpleFrame* child;
    void (*callback)(void*);
};
void SetRootProjection(CSimpleTop&);
class CollectScope final {
public:
    CollectScope();
    ~CollectScope();
    CollectScope(const CollectScope&) = delete;
    CollectScope& operator=(const CollectScope&) = delete;
};
class UpdateScope final {
public:
    UpdateScope();
    ~UpdateScope();
    UpdateScope(const UpdateScope&) = delete;
    UpdateScope& operator=(const UpdateScope&) = delete;
};
}}
#endif
