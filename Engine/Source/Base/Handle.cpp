#include <cstdint>
#include "Engine/Source/Base/Handle.h"
#include "Engine/Source/Storm/H/storm.h"
#if defined(WOWDECOMP_PORTABLE_RUNTIME)
#include <stdexcept>
#endif

// INLINED
CHandleObject::CHandleObject() {
    this->m_refcount = 0;
#if defined(WOWDECOMP_PORTABLE_RUNTIME)
    this->m_nativeOwner = std::this_thread::get_id();
#endif
}

#if defined(WOWDECOMP_PORTABLE_RUNTIME)
void CHandleObject::NativeCheckOwner() const {
    if (this->m_nativeOwner != std::this_thread::get_id())
        throw std::logic_error("Native handle requires its owner thread");
}
#endif

// INLINED
CHandleObject::~CHandleObject() {
}

// INLINED
const char* CHandleObject::GetObjectName() {
    return nullptr;
}

// VERIFY: baa00fb6 auto
// OFFSET: 0x0047BF20
int32_t HandleCompare(HOBJECT a, HOBJECT b) {
#if defined(WOWDECOMP_PORTABLE_RUNTIME)
    // Native handles are unrelated object addresses; pointer subtraction is UB.
    auto left = reinterpret_cast<uintptr_t>(a), right = reinterpret_cast<uintptr_t>(b);
    return (left > right) - (left < right);
#else
    return static_cast<int32_t>(a - b);
#endif
}

// VERIFY: 436d38d9 manual
// OFFSET: 0x0047BF30
void HandleClose(HOBJECT handle) {
    CHandleObject* object = HandleDereference(handle);

    if (object->m_refcount-- == 1) {
        delete object;
    }
}

// VERIFY: 96ec39c1 auto
// OFFSET: 0x0047BF50
HOBJECT HandleCreate(CHandleObject* ptr, const char* handleName) {
    if (!ptr) {
        SErrSetLastError(STORM_ERROR_INVALID_PARAMETER);
        return nullptr;
    }

#if defined(WOWDECOMP_PORTABLE_RUNTIME)
    ptr->NativeCheckOwner();
#endif
    ptr->m_refcount++;
    return reinterpret_cast<HOBJECT>(ptr);
}

// INLINED
CHandleObject* HandleDereference(HOBJECT handle) {
#if defined(WOWDECOMP_PORTABLE_RUNTIME)
    if (!handle) throw std::invalid_argument("Null native handle");
    reinterpret_cast<CHandleObject*>(handle)->NativeCheckOwner();
#endif
    return reinterpret_cast<CHandleObject*>(handle);
}

// VERIFY: 1d1aba75 manual
// OFFSET: 0x0047BF70
HOBJECT HandleDuplicate(HOBJECT handle) {
    if (handle) {
        HandleDereference(handle)->m_refcount++;
    }

    return handle;
}

void HandleDestroy() {
}

void HandleInitialize() {
}
