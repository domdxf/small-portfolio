#include "r_lua.h"

// more reconstructed implementations of the roblox lua api; extracted from IDA

const char *r_luaT_typenames[] = {
	"nil", "userdata", "number", "boolean",
	"string", "thread", "function", "table", "userdata",
	"proto", "upval"
};

int r_lua_equal(DWORD L, int index1, int index2) {
	TValue *o1 = (TValue *)r_index2adr(L, index1);
	TValue *o2 = (TValue *)r_index2adr(L, index2);
	int i = (o1 == luaO_nilobject || o2 == luaO_nilobject) ? 0 : r_equalobj(o1, o2);
	return i;
}

int r_lua_lessthan(DWORD L, int index1, int index2) {
	TValue *o1 = (TValue *)r_index2adr(L, index1);
	TValue *o2 = (TValue *)r_index2adr(L, index2);
	int i = (o1 == luaO_nilobject || o2 == luaO_nilobject) ? 0 : r_lessthanobj(o1, o2);
	return i;
}

int r_lua_gettop(DWORD L) {
	return (*(DWORD *)(L + 16) - *(DWORD *)(L + 28)) >> 4;
}

void r_lua_insert(DWORD L, int idx) {
	int v4;
	int v5;

	DWORD *v2 = r_index2adr(L, idx);
	unsigned int v3 = *(DWORD *)(L + 16);
	if (v3 > (unsigned int)v2) {
		v4 = v3 - 16;
		do {
			*(DWORD *)v3 = *(DWORD *)v4;
			*(DWORD *)(v3 + 4) = *(DWORD *)(v4 + 4);
			v3 -= 16;
			*(DWORD *)(v4 + 24) = *(DWORD *)(v4 + 8);
			v4 -= 16;
		} while (v3 > (unsigned int)v2);
	}
	v5 = *(DWORD *)(L + 16);
	*v2 = *(DWORD *)v5;
	v2[1] = *(DWORD *)(v5 + 4);
	v2[2] = *(DWORD *)(v5 + 8);
}

void r_lua_remove(DWORD L, int idx) {
	DWORD *v4;

	DWORD *v2 = r_index2adr(L, idx);
	unsigned int v3 = (unsigned int)(v2 + 4);
	if ((unsigned int)(v2 + 4) < *(DWORD *)(L + 16)) {
		v4 = v2;
		do {
			*v4 = *(DWORD *)v3;
			v4[1] = *(DWORD *)(v3 + 4);
			v4[2] = v4[6];
			v3 += 16;
			v4 += 4;
		} while (v3 < *(DWORD *)(L + 16));
	}
	r_api_decr_top(L);
}

void r_lua_pushnil(DWORD L) {
	*(DWORD *)(*(DWORD *)(L + 16) + 8) = 0;
	r_api_incr_top(L);
}

void r_lua_pushboolean(DWORD L, int b) {
	int v2 = *(DWORD *)(L + 16);
	*(DWORD *)(v2 + 8) = 3;
	*(DWORD *)v2 = b != 0;
	r_api_incr_top(L);
}

void r_lua_pushvalue(DWORD L, int idx) {
	DWORD *v2 = r_index2adr(L, idx);
	int v3 = *(DWORD *)(L + 16);
	*(DWORD *)v3 = *v2;
	*(DWORD *)(v3 + 4) = v2[1];
	*(DWORD *)(v3 + 8) = v2[2];
	r_api_incr_top(L);
}

void r_lua_settop(DWORD L, int idx) {
	int i;
	if (idx < 0) {
		*(DWORD *)(L + 16) += 16 * idx + 16;
	}
	else {
		for (i = 16 * idx; *(DWORD *)(L + 16) < (unsigned int)(i + *(DWORD *)(L + 28)); *(DWORD *)(L + 16) += 16)
			*(DWORD *)(*(DWORD *)(L + 16) + 8) = 0;
		*(DWORD *)(L + 16) = i + *(DWORD *)(L + 28);
	}
}

int r_lua_type(DWORD L, int idx) {
	StkId o = (TValue *)r_index2adr(L, idx);
	return (o == luaO_nilobject) ? LUA_TNONE : ttype(o);
}

const char *r_lua_typename(DWORD L, int t) {
	return (t == LUA_TNONE) ? "no value" : r_luaT_typenames[t];
}

int r_lua_iscfunction(DWORD L, int idx) {
	TValue *o = (TValue *)r_index2adr(L, idx);
	return (o->tt == R_LUA_TFUNCTION && o->value.gc->cl.c.isC);
}

int r_lua_isnumber(DWORD L, int idx) {
	TValue *n = new TValue;
	TValue *o = (TValue *)r_index2adr(L, idx);
	return (o->tt == R_LUA_TNUMBER || ((o = r_luaV_tonumber(o, n)) != NULL));
}

int r_lua_isstring(DWORD L, int idx) {
	int type = r_lua_type(L, idx);
	return (type == R_LUA_TSTRING || type == R_LUA_TNUMBER);
}

int r_lua_isuserdata(DWORD L, int idx) {
	TValue *o = (TValue *)r_index2adr(L, idx);
	return (o->tt == R_LUA_TLIGHTUSERDATA || o->tt == R_LUA_TUSERDATA);
}

int r_lua_toboolean(DWORD L, int idx) {
	TValue *o = (TValue *)r_index2adr(L, idx);
	return !r_l_isfalse(o);
}

const char *r_lua_tolstring(DWORD L, int idx, size_t *len) {
	lua_State *state = luaL_newstate();
	r_copystack(L, state);
	const char *value = lua_tolstring(state, idx, len);
	lua_close(state);
	return value;
}