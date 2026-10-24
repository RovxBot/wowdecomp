#ifndef WOWDECOMP_NATIVE_FRAME_BACKDROPS_H
#define WOWDECOMP_NATIVE_FRAME_BACKDROPS_H
struct lua_State;
namespace Frame { namespace Native {
int GetBackdrop(lua_State*);
int SetBackdrop(lua_State*);
int GetBackdropColor(lua_State*);
int SetBackdropColor(lua_State*);
int GetBackdropBorderColor(lua_State*);
int SetBackdropBorderColor(lua_State*);
}}
#endif
