#define lbitlib_c
#define LUA_LIB

#include "lua.h"

#include "lauxlib.h"
#include "lualib.h"


typedef int Integer;
typedef unsigned int UInteger;

#if defined(WOWDECOMP_PORTABLE_RUNTIME)
// Native conversion preserves the original x64 signed-int indefinite value.
static Integer checkinteger(lua_State* L, int index) {
  int result;
  lua_number2int(result, luaL_checknumber(L, index));
  return result;
}
#else
#define checkinteger(L,index) ((Integer)luaL_checknumber(L,index))
#endif


// VERIFY: d6a77d71 manual
// OFFSET: 0x00850CC0
static int bit_bnot (lua_State *L) {
  lua_pushnumber(L, (UInteger)~checkinteger(L, 1));
  return 1;
}


// VERIFY: e75ae7e6 manual
// OFFSET: 0x00850D00
static int bit_band (lua_State *L) {
  int n = lua_gettop(L), i;
  UInteger w = checkinteger(L, 1);
  for (i = 2; i <= n; i++)
    w &= checkinteger(L, i);
  lua_pushnumber(L, w);
  return 1;
}


// VERIFY: 2eb41a74 manual
// OFFSET: 0x00850D80
static int bit_bor (lua_State *L) {
  int n = lua_gettop(L), i;
  UInteger w = checkinteger(L, 1);
  for (i = 2; i <= n; i++)
    w |= checkinteger(L, i);
  lua_pushnumber(L, w);
  return 1;
}


// VERIFY: 6aea1192 manual
// OFFSET: 0x00850E00
static int bit_bxor (lua_State *L) {
  int n = lua_gettop(L), i;
  UInteger w = checkinteger(L, 1);
  for (i = 2; i <= n; i++)
    w ^= checkinteger(L, i);
  lua_pushnumber(L, w);
  return 1;
}


// VERIFY: 5453693c auto
// OFFSET: 0x00850E80
static int bit_lshift (lua_State *L) {
#if defined(WOWDECOMP_PORTABLE_RUNTIME)
  /* Original x86 count masking and uint32 wrapping, without signed-shift UB. */
  int value, count;
  lua_number2int(value, luaL_checknumber(L, 1));
  lua_number2int(count, luaL_checknumber(L, 2));
  lua_pushnumber(L, (UInteger)value << ((UInteger)count & 31));
#else
  lua_pushnumber(L, (UInteger)((Integer)luaL_checknumber(L, 1) << (UInteger)luaL_checknumber(L, 2)));
#endif
  return 1;
}


// VERIFY: 99a627b4 auto
// OFFSET: 0x00850EE0
static int bit_rshift (lua_State *L) {
#if defined(WOWDECOMP_PORTABLE_RUNTIME)
  int value, count;
  lua_number2int(value, luaL_checknumber(L, 1));
  lua_number2int(count, luaL_checknumber(L, 2));
  const UInteger result = (UInteger)value >> ((UInteger)count & 31);
  lua_pushnumber(L, result <= 2147483647U ? (lua_Number)result : (lua_Number)result - 4294967296.0);
#else
  lua_pushnumber(L, (Integer)((UInteger)luaL_checknumber(L, 1) >> (UInteger)luaL_checknumber(L, 2)));
#endif
  return 1;
}


// VERIFY: b56ebf4a auto
// OFFSET: 0x00850F40
static int bit_arshift (lua_State *L) {
#if defined(WOWDECOMP_PORTABLE_RUNTIME)
  int value, count;
  lua_number2int(value, luaL_checknumber(L, 1));
  lua_number2int(count, luaL_checknumber(L, 2));
  const UInteger shift = (UInteger)count & 31;
  UInteger result = (UInteger)value >> shift;
  if (value < 0 && shift) result |= ~0U << (32 - shift);
  lua_pushnumber(L, result <= 2147483647U ? (lua_Number)result : (lua_Number)result - 4294967296.0);
#else
  lua_pushnumber(L, (Integer)luaL_checknumber(L, 1) >> (UInteger)luaL_checknumber(L, 2));
#endif
  return 1;
}


// VERIFY: 8e98a529 auto
// OFFSET: 0x00850F90
static int bit_mod (lua_State *L) {
  if (checkinteger(L, 2) == 0) {
    lua_pushnumber(L, 1 / lua_tonumber(L, 2));
    return 1;
  }
#if defined(WOWDECOMP_PORTABLE_RUNTIME)
  const Integer dividend = checkinteger(L, 1), divisor = checkinteger(L, 2);
  if (dividend == (-2147483647 - 1) && divisor == -1)
    return luaL_error(L, "integer remainder overflow");
  lua_pushnumber(L, dividend % divisor);
#else
  lua_pushnumber(L, checkinteger(L, 1) % checkinteger(L, 2));
#endif
  return 1;
}


static const struct luaL_Reg bitlib[] = {
  {"bnot", bit_bnot},
  {"band", bit_band},
  {"bor", bit_bor},
  {"bxor", bit_bxor},
  {"lshift", bit_lshift},
  {"rshift", bit_rshift},
  {"arshift", bit_arshift},
  {"mod", bit_mod},
  {NULL, NULL}
};


// VERIFY: 23d41808 auto
// OFFSET: 0x00851010
LUALIB_API int luaopen_bit (lua_State *L) {
  luaL_openlib(L, "bit", bitlib, 0);
  return 1;
}
