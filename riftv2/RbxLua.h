#pragma once

#include <Windows.h>
#include <string>


/* CONFIG DEFINITIONS */

// Base/Top: Inside settop (55 8B EC 8B 55 0C 85 D2 78 38); third line of the >= 0 condition
//           *(_DWORD *)(a1 + 32) = i + *(_DWORD *)(a1 + 12);    top = index + base
#define RLUAST_TOP		24
#define RLUAST_BASE		16

/* CONSTANT DEFINITIONS */

#define RLUA_REGISTRYINDEX       (-10000)
#define RLUA_ENVIRONINDEX        (-10001)
#define RLUA_GLOBALSINDEX        (-10002)
#define rlua_upvalueindex(i)     (RLUA_GLOBALSINDEX-(i))

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

#define rlua_setglobal(L,s)     rlua_setfield(L, RLUA_GLOBALSINDEX, (s))
#define rlua_getglobal(L,s)     rlua_getfield(L, RLUA_GLOBALSINDEX, (s))
#define rlua_tostring(L,i)      rlua_tolstring(L, (i), NULL)
#define rlua_register(L,n,f)	(rlua_pushcfunction(L, (f)), rlua_setglobal(L, (n)))
#define rlua_newtable(L)        rlua_createtable(L, 0, 0)
#define rlua_pop(L,n)           rlua_settop(L, -(n)-1)

#define rlua_isfunction(L,n)		(rlua_type(L, (n)) == RLUA_TFUNCTION)
#define rlua_istable(L,n)			(rlua_type(L, (n)) == RLUA_TTABLE)
#define rlua_islightuserdata(L,n)   (rlua_type(L, (n)) == RLUA_TLIGHTUSERDATA)
#define rlua_isnil(L,n)				(rlua_type(L, (n)) == RLUA_TNIL)
#define rlua_isboolean(L,n)			(rlua_type(L, (n)) == RLUA_TBOOLEAN)
#define rlua_isthread(L,n)			(rlua_type(L, (n)) == RLUA_TTHREAD)
#define rlua_isnone(L,n)			(rlua_type(L, (n)) == RLUA_TNONE)
#define rlua_isnoneornil(L, n)		(rlua_type(L, (n)) <= 0)


const char *const rluaT_typenames[] = {
	"nil", "userdata", "number", "boolean",
	"string", "thread", "function", "table", "userdata",
	"proto", "upval"
};

typedef int(*rlua_CFunction)(DWORD RL);


/* SCANNED FUNCTIONS */

typedef DWORD*(__cdecl *_rindex2adr)(DWORD RL, int idx);
typedef void(__cdecl *_sandboxthread)(DWORD RL);
typedef void(*_deserialize)(DWORD RL, std::string& code, const char *chunkname, unsigned int modkey);
typedef void(*_printdevconsole)(int type, const char *format, ...);

typedef void(__cdecl *_rlua_getfield)(DWORD RL, int idx, const char *k);
typedef void(__cdecl *_rlua_setfield)(DWORD RL, int idx, const char *k);
typedef void(__cdecl *_rlua_settable)(DWORD RL, int idx);
typedef int(__cdecl *_rlua_getmetatable)(DWORD RL, int idx);
typedef int(__cdecl *_rlua_setmetatable)(DWORD RL, int idx);
typedef void(__cdecl *_rlua_createtable)(DWORD RL, int narray, int nrec);
typedef DWORD(__cdecl *_rlua_newthread)(DWORD RL);
typedef void*(__cdecl *_rlua_newuserdata)(DWORD RL, size_t size);
typedef void(__cdecl *_rlua_pushcclosure)(DWORD RL, int fn, int n);
typedef void(__cdecl *_rlua_pushstring)(DWORD RL, const char *s);
typedef void(__cdecl *_rlua_pushnumber)(DWORD RL, double n);
typedef const char*(__cdecl *_rlua_tolstring)(DWORD RL, int idx, size_t *len);
typedef void*(__cdecl *_rlua_topointer)(DWORD, int idx);
typedef double(__cdecl *_rlua_tonumber)(DWORD, int idx);
typedef int(__cdecl *_rlua_pcall)(DWORD RL, int nargs, int nresults, int errfunc);
typedef int(__cdecl *_rlua_resume)(DWORD RL, int narg);
typedef int(__cdecl *_rlua_next)(DWORD RL, int idx);
typedef void(__cdecl *_rlua_replace)(DWORD RL, int idx);
typedef int(__cdecl *_rlua_error)(DWORD RL);

_rindex2adr rindex2adr;
_sandboxthread sandboxthread;
_deserialize deserialize;
_printdevconsole printdevconsole;

_rlua_getfield rlua_getfield;
_rlua_setfield rlua_setfield;
_rlua_settable rlua_settable;
_rlua_getmetatable rlua_getmetatable;
_rlua_setmetatable rlua_setmetatable;
_rlua_createtable rlua_createtable;
_rlua_newthread rlua_newthread;
_rlua_newuserdata rlua_newuserdata;
_rlua_pushcclosure rlua_pushcclosure;
_rlua_pushstring rlua_pushstring;
_rlua_pushnumber rlua_pushnumber;
_rlua_tolstring rlua_tolstring;
_rlua_topointer rlua_topointer;
_rlua_tonumber rlua_tonumber;
_rlua_pcall rlua_pcall;
_rlua_resume rlua_resume;
_rlua_next rlua_next;
_rlua_replace rlua_replace;
_rlua_error rlua_error;

/* FUNCTION IMPLEMENTATIONS */

int rlua_gettop(DWORD RL) {
	return (*(DWORD*)(RL + RLUAST_TOP) - *(DWORD*)(RL + RLUAST_BASE)) >> 4;
}

void rlua_settop(DWORD RL, int idx) {
	DWORD i;
	if (idx < 0) {
		*(DWORD*)(RL + RLUAST_TOP) += 16 * idx + 16;
	} else {
		for (i = 16 * idx; *(DWORD*)(RL + RLUAST_TOP) < (unsigned int)(i + *(DWORD*)(RL + RLUAST_BASE)); *(DWORD*)(RL + RLUAST_TOP) += 16)
			*(DWORD*)(*(DWORD*)(RL + RLUAST_TOP) + 8) = 0;
		*(DWORD*)(RL + RLUAST_TOP) = i + *(DWORD*)(RL + RLUAST_BASE);
	}
}

void rlua_pushnil(DWORD RL) {
	*(DWORD*)(*(DWORD*)(RL + RLUAST_TOP) + 8) = 0;
	*(DWORD*)(RL + RLUAST_TOP) += 16;
}

void rlua_pushboolean(DWORD RL, int b) {
	DWORD top = *(DWORD*)(RL + RLUAST_TOP);
	*(DWORD *)top = b != 0;
	*(DWORD *)(top + 8) = RLUA_TBOOLEAN;
	*(DWORD *)(RL + RLUAST_TOP) += 16;
}

void rlua_pushvalue(DWORD RL, int idx) {
	DWORD *obj = rindex2adr(RL, idx);
	DWORD top = *(DWORD*)(RL + RLUAST_TOP);
	*(DWORD*)top = *obj;
	*(DWORD*)(top + 4) = obj[1];
	*(DWORD*)(top + 8) = obj[2];
	*(DWORD*)(RL + RLUAST_TOP) += 16;
}

int rlua_toboolean(DWORD RL, int idx) {
	DWORD *obj = rindex2adr(RL, idx);
	int value = obj[2];
	return value && (value != 3 || *obj);
}

void rlua_remove(DWORD RL, int idx) {
	DWORD *obj = rindex2adr(RL, idx);
	unsigned int v3 = (unsigned int)(obj + 4);
	if ((unsigned int)(obj + 4) < *(DWORD*)(RL + RLUAST_TOP)) {
		DWORD *v4 = obj;
		do {
			v4 += 4;
			*(v4 - 4) = *(DWORD*)v3;
			int v5 = *(DWORD*)(v3 + 4);
			v3 += 16;
			*(v4 - 3) = v5;
			*(v4 - 2) = v4[2];
		} while (v3 < *(DWORD*)(RL + RLUAST_TOP));
	}
	*(DWORD*)(RL + RLUAST_TOP) -= 16;
}

void rlua_insert(DWORD RL, int idx) {
	DWORD *obj = rindex2adr(RL, idx);
	for (DWORD i = *(DWORD*)(RL + RLUAST_TOP); i > (DWORD)obj; i -= 16) {
		*(DWORD*)i = *(DWORD*)(i - 16);
		*(DWORD*)(i + 4) = *(DWORD*)(i - 12);
		*(DWORD*)(i + 8) = *(DWORD*)(i - 8);
	}
	DWORD top = *(DWORD*)(RL + RLUAST_TOP);
	*obj = *(DWORD*)top;
	obj[1] = *(DWORD*)(top + 4);
	obj[2] = *(DWORD*)(top + 8);
}

void rlua_xmove(DWORD from, DWORD to, int n) {
	if (from != to) {
		*(DWORD*)(from + RLUAST_TOP) += -16 * n;
		if (n > 0) {
			DWORD v5 = 0;
			DWORD v9 = 0;
			do {
				DWORD v6 = *(DWORD*)(to + RLUAST_TOP);
				DWORD v7 = v5 + *(DWORD*)(from + RLUAST_TOP);
				*(DWORD*)(to + RLUAST_TOP) = v6 + 16;
				*(DWORD*)v6 = *(DWORD*)v7;
				*(DWORD*)(v6 + 4) = *(DWORD*)(v7 + 4);
				*(DWORD*)(v6 + 8) = *(DWORD*)(v7 + 8);
				v5 = v9 + 16;
				v9 += 16;
				--n;
			} while (n);
		}
	}
}

int rlua_type(DWORD RL, int idx) {
	DWORD *obj = rindex2adr(RL, idx);
	return (obj == NULL) ? RLUA_TNONE : obj[2];
}

const char *rlua_typename(DWORD RL, int t) {
	return (t == RLUA_TNONE) ? "no value" : rluaT_typenames[t];
}

/* CUSTOM IMPLEMENTATIONS */

void rlua_shallowcopytable(DWORD RL, int idx) {
	rlua_newtable(RL);
	rlua_pushnil(RL);
	while (rlua_next(RL, idx - 2) != 0) {
		rlua_pushvalue(RL, -2);
		rlua_insert(RL, -2);
		rlua_settable(RL, -4);
	}
}