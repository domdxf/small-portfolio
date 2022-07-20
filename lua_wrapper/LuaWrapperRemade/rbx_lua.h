#pragma once

#include <Windows.h>

extern "C" {
	#include "lua.h"
	#include "lualib.h"
	#include "lauxlib.h"
}

/* LUA FILES ARE EXEMPT FROM NAMING CONVENTIONS */

extern DWORD RLuaState;


#define RLUA_TNONE               (-1)
#define RLUA_TNIL                0
#define RLUA_TLIGHTUSERDATA      1
#define RLUA_TNUMBER             2
#define RLUA_TBOOLEAN            3
#define RLUA_TSTRING             4
#define RLUA_TTHREAD             5
#define RLUA_TFUNCTION           6
#define RLUA_TTABLE              7
#define RLUA_TUSERDATA           8
#define RLUA_TPROTO				 9
#define RLUA_TUPVAL				 10

#define rlua_setglobal(L,s)      rlua_setfield(L, -10002, (s))
#define rlua_getglobal(L,s)      rlua_getfield(L, -10002, (s))
#define rlua_tostring(L,i)       rlua_tolstring(L, (i), NULL)
#define rlua_register(L,n,f)	 (rlua_pushcclosure(L, f, 0), rlua_setglobal(L, n))
#define rlua_pop(L,n)            rlua_settop(L, -(n)-1)
#define rlua_newtable(L)         rlua_createtable(L, 0, 0)

typedef int(*rlua_CFunction)(DWORD RL);

const char *const rluaT_typenames[] = {
	"nil", "userdata", "number", "boolean",
	"string", "thread", "function", "table", "userdata",
	"proto", "upval"
};

typedef void(__cdecl *_rlua_getfield)(DWORD RL, int idx, const char *k);
typedef void(__cdecl *_rlua_setfield)(DWORD RL, int idx, const char *k);
typedef void(__cdecl *_rlua_rawgeti)(DWORD RL, int idx, int n);
typedef void(__cdecl *_rlua_rawseti)(DWORD RL, int idx, int n);
typedef int(__cdecl *_rlua_getmetatable)(DWORD RL, int idx);
typedef int(__cdecl *_rlua_setmetatable)(DWORD RL, int idx);
typedef void(__cdecl *_rlua_createtable)(DWORD RL, int narray, int nrec);
typedef DWORD(__cdecl *_rlua_newthread)(DWORD RL);
typedef void*(__cdecl *_rlua_newuserdata)(DWORD RL, size_t size);
typedef void(__cdecl *_rlua_pushcclosure)(DWORD RL, int fn, int n);
typedef void(__cdecl *_rlua_pushstring)(DWORD RL, const char *s);
typedef void(__cdecl *_rlua_pushnil)(DWORD RL);
typedef void(__cdecl *_rlua_pushnumber)(DWORD RL, double n);
typedef void(__cdecl *_rlua_pushvalue)(DWORD RL, int idx);
typedef void(__cdecl *_rlua_pushboolean)(DWORD RL, int b);
typedef void(__cdecl *_rlua_pushlightuserdata)(DWORD RL, void *p);
typedef const char*(__cdecl *_rlua_tolstring)(DWORD RL, int idx, size_t *len);
typedef void*(__cdecl *_rlua_topointer)(DWORD, int idx);
typedef double(__cdecl *_rlua_tonumber)(DWORD, int idx);
typedef int(__cdecl *_rlua_toboolean)(DWORD, int idx);
typedef void(__cdecl *_rlua_settop)(DWORD RL, int idx);
typedef void(__cdecl *_rlua_remove)(DWORD RL, int idx);
typedef int(__cdecl *_rlua_type)(DWORD RL, int idx);
typedef int(__cdecl *_rlua_pcall)(DWORD RL, int nargs, int nresults, int errfunc);
typedef void(__cdecl *_rlua_xmove)(DWORD from, DWORD to, int n);
typedef int(__cdecl *_rlua_next)(DWORD RL, int idx);
typedef int(__cdecl *_rluaL_ref)(DWORD RL, int t);
typedef void(__cdecl *_rluaL_unref)(DWORD RL, int t, int ref);
typedef int(__cdecl *_rluaL_getmetafield)(DWORD RL, int obj, const char *e);
typedef int(__cdecl *_rluaD_poscall)(DWORD RL, DWORD firstResult);
typedef void(__cdecl *_sandboxthread)(DWORD RL);

extern _rlua_getfield rlua_getfield;
extern _rlua_setfield rlua_setfield;
extern _rlua_rawgeti rlua_rawgeti;
extern _rlua_rawseti rlua_rawseti;
extern _rlua_getmetatable rlua_getmetatable;
extern _rlua_setmetatable rlua_setmetatable;
extern _rlua_createtable rlua_createtable;
extern _rlua_newthread rlua_newthread;
extern _rlua_newuserdata rlua_newuserdata;
extern _rlua_pushcclosure rlua_pushcclosure;
extern _rlua_pushstring rlua_pushstring;
extern _rlua_pushnil rlua_pushnil;
extern _rlua_pushnumber rlua_pushnumber;
extern _rlua_pushvalue rlua_pushvalue;
extern _rlua_pushboolean rlua_pushboolean;
extern _rlua_pushlightuserdata rlua_pushlightuserdata;
extern _rlua_tolstring rlua_tolstring;
extern _rlua_topointer rlua_topointer;
extern _rlua_tonumber rlua_tonumber;
extern _rlua_toboolean rlua_toboolean;
extern _rlua_settop rlua_settop;
extern _rlua_remove rlua_remove;
extern _rlua_pcall rlua_pcall;
extern _rlua_type rlua_type;
extern _rlua_xmove rlua_xmove;
extern _rlua_next rlua_next;
extern _rluaL_ref rluaL_ref;
extern _rluaL_unref rluaL_unref;
extern _rluaL_getmetafield rluaL_getmetafield;
extern _rluaD_poscall rluaD_poscall;
extern _sandboxthread sandboxthread;

const char *rlua_typename(DWORD RL, int t);
int rlua_gettop(DWORD RL);