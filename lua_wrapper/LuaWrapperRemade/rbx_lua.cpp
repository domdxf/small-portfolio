#include "rbx_lua.h"

DWORD RLuaState;

_rlua_getfield rlua_getfield;
_rlua_setfield rlua_setfield;
_rlua_rawgeti rlua_rawgeti;
_rlua_rawseti rlua_rawseti;
_rlua_getmetatable rlua_getmetatable;
_rlua_setmetatable rlua_setmetatable;
_rlua_createtable rlua_createtable;
_rlua_newthread rlua_newthread;
_rlua_newuserdata rlua_newuserdata;
_rlua_pushcclosure rlua_pushcclosure;
_rlua_pushstring rlua_pushstring;
_rlua_pushnil rlua_pushnil;
_rlua_pushnumber rlua_pushnumber;
_rlua_pushvalue rlua_pushvalue;
_rlua_pushboolean rlua_pushboolean;
_rlua_pushlightuserdata rlua_pushlightuserdata;
_rlua_tolstring rlua_tolstring;
_rlua_topointer rlua_topointer;
_rlua_tonumber rlua_tonumber;
_rlua_toboolean rlua_toboolean;
_rlua_settop rlua_settop;
_rlua_remove rlua_remove;
_rlua_pcall rlua_pcall;
_rlua_type rlua_type;
_rlua_xmove rlua_xmove;
_rlua_next rlua_next;
_rluaL_ref rluaL_ref;
_rluaL_unref rluaL_unref;
_rluaL_getmetafield rluaL_getmetafield;
_rluaD_poscall rluaD_poscall;
_sandboxthread sandboxthread;

const char *rlua_typename(DWORD RL, int t) {
	return (t == RLUA_TNONE) ? "no value" : rluaT_typenames[t];
}

int rlua_gettop(DWORD RL) {
	return (*(DWORD*)(RL + 12) - *(DWORD*)(RL + 20)) >> 4;
}