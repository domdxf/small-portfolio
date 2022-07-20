#pragma once
#pragma warning(disable:4244)

#include <Windows.h>
#include <iostream>

#include "luadefs.h"

#define r_concat				0x14B470 
#define r_createtable			0x14B530 
#define r_getfield				0x14B890
#define r_setfield				0x14D3F0
#define r_pcall					0x14C580
#define r_pushstring			0x14CAE0 
#define r_pushnumber			0x14CA60 
#define r_newthread				0x14C280 
#define r_objlen				0x14C4A0
#define r_pushcclosure			0x14C6B0 
#define r_rawseti				0x14CF50 
#define r_rawgeti				0x14CD80 
#define r_tonumber				0x14DC60 
#define r_replace				0x14D100
#define r_setthreadidentity		0x1399A0

#define r_lua_getglobal(L,s)		r_lua_getfield(L, LUA_GLOBALSINDEX, (s))
#define r_lua_setglobal(L,s)		r_lua_setfield(L, LUA_GLOBALSINDEX, (s))
#define r_lua_pop(L,n)				r_lua_settop(L, -(n)-1)
#define r_lua_emptystack(L)			r_lua_settop(L, 0)
#define r_lua_newtable(L)			r_lua_createtable(L, 0, 0)
#define r_lua_register(L,n,f)		(r_lua_pushcfunction(L, (f)), r_lua_setglobal(L, (n)))
#define r_lua_pushcfunction(L,f)	r_lua_pushcclosure(L, (f), 0)
#define r_lua_strlen(L,i)			r_lua_objlen(L, (i))
#define r_lua_tostring(L,i)			r_lua_tolstring(L, (i), NULL)

#define r_lua_isfunction(L,n)		(r_lua_type(L, (n)) == R_LUA_TFUNCTION)
#define r_lua_istable(L,n)			(r_lua_type(L, (n)) == R_LUA_TTABLE)
#define r_lua_islightuserdata(L,n)  (r_lua_type(L, (n)) == R_LUA_TLIGHTUSERDATA)
#define r_lua_isnil(L,n)			(r_lua_type(L, (n)) == R_LUA_TNIL)
#define r_lua_isboolean(L,n)		(r_lua_type(L, (n)) == R_LUA_TBOOLEAN)
#define r_lua_isthread(L,n)			(r_lua_type(L, (n)) == R_LUA_TTHREAD)
#define r_lua_isnone(L,n)			(r_lua_type(L, (n)) == R_LUA_TNONE)
#define r_lua_isnoneornil(L, n)		(r_lua_type(L, (n)) <= 0)

int r_lua_equal(DWORD L, int index1, int index2);
int r_lua_lessthan(DWORD L, int index1, int index2);
int r_lua_gettop(DWORD L);
void r_lua_insert(DWORD L, int idx);
void r_lua_remove(DWORD L, int idx);
void r_lua_pushnil(DWORD L);
void r_lua_pushboolean(DWORD L, int b);
void r_lua_pushvalue(DWORD L, int idx);
void r_lua_settop(DWORD L, int idx);
int r_lua_type(DWORD L, int idx);
const char *r_lua_typename(DWORD L, int t);
int r_lua_iscfunction(DWORD L, int idx);
int r_lua_isnumber(DWORD L, int idx);
int r_lua_isstring(DWORD L, int idx);
int r_lua_isuserdata(DWORD L, int idx);
int r_lua_toboolean(DWORD L, int idx);
const char *r_lua_tolstring(DWORD L, int idx, size_t *len);

typedef void(__cdecl *PROTO_concat)(DWORD L, int n);
typedef void(__cdecl *PROTO_createtable)(DWORD L, int narr, int nrec);
typedef void(__cdecl *PROTO_getfield)(DWORD L, int idx, const char *k);
typedef void(__cdecl *PROTO_setfield)(DWORD L, int idx, const char *k);
typedef int(__cdecl *PROTO_pcall)(DWORD L, int nargs, int nresults, int errfunc);
typedef void(__cdecl *PROTO_pushstring)(DWORD L, const char *s);
typedef void(__cdecl *PROTO_pushnumber)(DWORD L, double n);
typedef int(__cdecl *PROTO_newthread)(DWORD L);
typedef int(__cdecl *PROTO_objlen)(DWOpushvalueRD L, int idx);
typedef void(__cdecl *PROTO_pushcclosure)(DWORD L, int fn, int n);
typedef void(__cdecl *PROTO_rawseti)(DWORD L, int idx, int n);
typedef void(__cdecl *PROTO_rawgeti)(DWORD L, int idx, int n);
typedef double(__cdecl *PROTO_tonumber)(DWORD L, int idx);
typedef void(__cdecl *PROTO_replace)(DWORD L, int idx);
typedef void(__cdecl *PROTO_setthreadidentity)(DWORD L, int i, int *b);

static PROTO_concat r_lua_concat;
static PROTO_createtable r_lua_createtable;
static PROTO_getfield r_lua_getfield;
static PROTO_setfield r_lua_setfield;
static PROTO_pcall r_lua_pcall;
static PROTO_pushstring r_lua_pushstring;
static PROTO_pushnumber r_lua_pushnumber;
static PROTO_newthread r_lua_newthread;
static PROTO_objlen r_lua_objlen;
static PROTO_pushcclosure r_lua_pushcclosure;
static PROTO_rawseti r_lua_rawseti;
static PROTO_rawgeti r_lua_rawgeti;
static PROTO_tonumber r_lua_tonumber;
static PROTO_replace r_lua_replace;
static PROTO_setthreadidentity setthreadidentity;