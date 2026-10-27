#ifndef WOWDECOMP_NATIVE_FRAME_HIERARCHY_H
#define WOWDECOMP_NATIVE_FRAME_HIERARCHY_H
#include "Regions.h"
class CSimpleTop;
class CSimpleTitleRegion;
class CSimpleTexture;
namespace Frame { namespace Native {
// Frames are owned by their parent/root. Heap and selected pool allocations may
// coexist. Borrow until destruction; XML/Lua factory registration is separate.
void RequireRoot();
using ScrollNotify = void (*)(CSimpleFrame*);
void SetScrollNotify(ScrollNotify);
bool HasScrollSupport();
void NotifyScroll(CSimpleFrame*);
void RequireScriptTextMutation();
// Only the original scroll-range notification may update qualified existing
// controls while layer traversal is pinned. No tree/layout/deletion capability.
struct ScrollRangeEdits final {
    ScrollRangeEdits();
    ~ScrollRangeEdits();
    ScrollRangeEdits(const ScrollRangeEdits&) = delete;
    ScrollRangeEdits& operator=(const ScrollRangeEdits&) = delete;
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
