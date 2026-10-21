#ifndef ENGINE_SOURCE_FRAME_SIMPLEFRAMESCRIPT_H
#define ENGINE_SOURCE_FRAME_SIMPLEFRAMESCRIPT_H

#include <cstdint>
struct lua_State;
int32_t SimpleFrameScript_CreateFont(lua_State*);

void RegisterSimpleFrameScriptMethods();
void UnregisterSimpleFrameScriptMethods();

#endif
