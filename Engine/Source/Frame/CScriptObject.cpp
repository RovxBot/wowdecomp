#ifdef WOWDECOMP_PORTABLE_RUNTIME
#include "Engine/Source/Frame/Portable/Fonts.h"
#define FRAME_FONT_CHECK() Frame::Native::RequireFontFactories()
#else
#define FRAME_FONT_CHECK() ((void)0)
#endif
#include <stdexcept>
#include <cstddef>
#include <cstdint>
#include "Engine/Source/Frame/CScriptObject.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/Storm/H/storm.h"
#include "Engine/Source/lua-5.1/src/lua.h"

// STATIC: 0x00B4997C
int32_t CScriptObject::s_objectType;

// STATIC: 0x00AC1024
const char* CScriptObject::s_objectTypeName = "Object";

// INLINED
int32_t CScriptObject::GetObjectType() {
    FRAME_FONT_CHECK();
    if (!CScriptObject::s_objectType) {
        CScriptObject::s_objectType = ++FrameScript_Object::s_objectTypes;
    }

    return CScriptObject::s_objectType;
}

// VERIFY: f2b689c9 auto
// OFFSET: 0x00431E80
const char* CScriptObject::GetName() {
    FRAME_FONT_CHECK();
    return this->m_name.m_str;
}

// VERIFY: e5cfe9cd auto
// OFFSET: 0x00482AF0
bool CScriptObject::IsA(int32_t type) {
    FRAME_FONT_CHECK();
    return type == CScriptObject::s_objectType;
}

// VERIFY: e9087c82 manual
// OFFSET: 0x00482B10
bool CScriptObject::IsA(const char* typeName) {
    FRAME_FONT_CHECK();
    return SStrCmpI(typeName, CScriptObject::s_objectTypeName, STORM_MAX_STR) == 0;
}

// VERIFY: 47f143d5 auto
// OFFSET: 0x0048B540
void CScriptObject::CreateName(const char* source, char* dest, uint32_t destSize) {
    FRAME_FONT_CHECK();
    if (SStrCmpI(source, "$parent", SStrLen("$parent"))) {
        SStrCopy(dest, source, destSize);
        return;
    }

#ifdef WOWDECOMP_PORTABLE_RUNTIME
    SStrCopy(dest, "Top", destSize);
#else
    SStrCopy(dest, "Top", STORM_MAX_STR);
#endif

#ifdef WOWDECOMP_PORTABLE_RUNTIME
    size_t depth = 0;
#endif
    for (CScriptObject* parent = this->GetScriptObjectParent(); parent; parent = parent->GetScriptObjectParent()) {
#ifdef WOWDECOMP_PORTABLE_RUNTIME
        Frame::Native::RequireScript(parent);
        if (++depth > 64) throw std::invalid_argument("Cyclic or excessive script parent chain");
#endif
        const char* name = parent->GetName();

        if (name && *name) {
            SStrCopy(dest, name, destSize);
            break;
        }
    }

    SStrPack(dest, source + SStrLen("$parent"), destSize);
}

// VERIFY: 2ff9c5ff auto
// OFFSET: 0x0048B5F0
CScriptObject* CScriptObject::GetScriptObjectByName(const char* name, int32_t type) {
    FRAME_FONT_CHECK();
    lua_State* L = FrameScript_GetContext();

    lua_pushstring(L, name);
    lua_rawget(L, LUA_GLOBALSINDEX);

    if (lua_type(L, -1) != LUA_TTABLE) {
        lua_settop(L, -2);
        return nullptr;
    }

#ifdef WOWDECOMP_PORTABLE_RUNTIME
    CScriptObject* object = dynamic_cast<CScriptObject*>(FrameScript::Native::GetObject(L, lua_gettop(L)));
    lua_pop(L, 1);
#else
    lua_rawgeti(L, -1, 0);
    CScriptObject* object = static_cast<CScriptObject*>(lua_touserdata(L, -1));
    lua_settop(L, -3);
#endif

    if (object && object->IsA(type)) {
        return object;
    }

    return nullptr;
}

// VERIFY: fc546e73 manual
// OFFSET: 0x0048B680
CScriptObject::CScriptObject() {
    FRAME_FONT_CHECK();
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::ScriptCreated(this);
#endif
}

// VERIFY: edffba21 auto
// OFFSET: 0x0048B6B0
const char* CScriptObject::GetObjectTypeName() {
    FRAME_FONT_CHECK();
    return CScriptObject::s_objectTypeName;
}

// VERIFY: 4a318446 auto
// OFFSET: 0x0048B6C0
void CScriptObject::SetName(const char* name) {
    FRAME_FONT_CHECK();
    if (this->m_name.m_str) {
        this->UnregisterScriptObject(this->m_name.m_str);
        this->m_name.Copy(nullptr);
    }

    if (name && *name) {
        char fullName[1024];
        this->CreateName(name, fullName, sizeof(fullName));
        this->m_name.Copy(fullName);
        this->RegisterScriptObject(fullName);
    }
}

// VERIFY: 09d59e9f manual
// OFFSET: [0x0048B730, 0x0048B760]
CScriptObject::~CScriptObject() {
    FRAME_FONT_CHECK();
#ifdef WOWDECOMP_PORTABLE_RUNTIME
    Frame::Native::ScriptDestroyed(this);
#endif
    if (this->m_name.m_str) {
        this->UnregisterScriptObject(this->m_name.m_str);
        this->m_name.Copy(nullptr);
    }
}
