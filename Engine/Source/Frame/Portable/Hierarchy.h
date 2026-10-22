#ifndef WOWDECOMP_NATIVE_FRAME_HIERARCHY_H
#define WOWDECOMP_NATIVE_FRAME_HIERARCHY_H
#include "Regions.h"
class CSimpleTop;
class CSimpleTitleRegion;
namespace Frame { namespace Native {
// Direct C++ Frames are owned by their parent/root. Borrow them until destruction;
// heap allocation is required, and no native XML/Lua Frame factory is advertised.
void RequireRoot();
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
