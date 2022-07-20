#pragma once

#include <Windows.h>
#include <iostream>
#include <string>

extern "C" {
	#include "lua.h"
	#include "lauxlib.h"
	#include "lualib.h"
	#include "lobject.h"
	#include "lstate.h"
	#include "llimits.h"
	#include "ldo.h"
	#include "lstring.h"
	#include "lvm.h"
	#include "ltable.h"
}

#define R_LUA_TNIL				0
#define R_LUA_TLIGHTUSERDATA	1
#define R_LUA_TNUMBER			2
#define R_LUA_TBOOLEAN			3
#define R_LUA_TSTRING			4
#define R_LUA_TTHREAD			5
#define R_LUA_TFUNCTION			6
#define R_LUA_TTABLE			7
#define R_LUA_TUSERDATA			8
#define R_LUA_TPROTO			9
#define R_LUA_TUPVAL			10

#define api_incr_top(L)   {api_check(L, L->top < L->ci->top); L->top++;}

#define r_api_incr_top(L)   *(DWORD *)(L + 16) += 16
#define r_api_decr_top(L)	*(DWORD *)(L + 16) -= 16;

#define r_equalobj(o1,o2) \
        (ttype(o1) == ttype(o2) && r_luaV_equalval(o1, o2))
#define r_lessthanobj(o1,o2) \
		(ttype(o1) == ttype(o2) && r_luaV_lessthan(o1, o2))
#define r_l_isfalse(o)	(o->tt == R_LUA_TNIL || (o->tt == R_LUA_TBOOLEAN && o->value.b == 0))

extern const char *r_luaT_typenames[];

void r_luaL_error(DWORD L, std::string str);
std::string lua_syntaxcheck(std::string code);
int lua_execute(lua_State *state, std::string code);

DWORD *r_index2adr(DWORD L, int idx);
static int l_strcmp(const TString *ls, const TString *rs);
TValue *r_luaV_tonumber(TValue *obj, TValue *n);
int r_luaV_tostring(StkId obj);
int r_luaV_equalval(const TValue *t1, const TValue *t2);
int r_luaV_lessthan(const TValue *t1, const TValue *t2);
void r_copyvalue(DWORD L, lua_State *L2, int idx);
void r_copystack(DWORD L, lua_State *L2);
int l2rtype(int type);