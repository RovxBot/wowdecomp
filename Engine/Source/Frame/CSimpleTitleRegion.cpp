#ifdef WOWDECOMP_PORTABLE_RUNTIME
#include "Engine/Source/Frame/Portable/Scripts.h"
#endif
#include <cstddef>
#include <cstdint>
#include "Engine/Source/Frame/CSimpleTitleRegion.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"

// STATIC: 0x00AC1548
int32_t CSimpleTitleRegion::s_metatable = LUA_NOREF;

// VERIFY: dfa639a0 auto
// OFFSET: 0x0048F060
int32_t CSimpleTitleRegion::GetScriptMetaTable() {
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::RequireRegionScripts();
#endif
    return CSimpleTitleRegion::s_metatable;
}
