#ifndef ENGINE_SOURCE_BASE_HANDLE_H
#define ENGINE_SOURCE_BASE_HANDLE_H

#include <cstdint>
#include "Engine/Source/Base/Types.h"
#if defined(WOWDECOMP_PORTABLE_RUNTIME)
#include <thread>
#endif

class CHandleObject {
    public:
    /* 0004 */ int32_t m_refcount;

    virtual ~CHandleObject();
    virtual const char* GetObjectName();

    CHandleObject();
#if defined(WOWDECOMP_PORTABLE_RUNTIME)
    // Native runtime objects retain their construction thread; original layout
    // annotations and Windows ABI remain unchanged outside this native branch.
    void NativeCheckOwner() const;
    private:
    std::thread::id m_nativeOwner;
#endif
};

void HandleClose(HOBJECT handle);
int32_t HandleCompare(HOBJECT a, HOBJECT b);
HOBJECT HandleCreate(CHandleObject* ptr, const char* handleName);
CHandleObject* HandleDereference(HOBJECT handle);
void HandleDestroy();
HOBJECT HandleDuplicate(HOBJECT handle);
void HandleInitialize();

#endif
