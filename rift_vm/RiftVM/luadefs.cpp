#include "luadefs.h"

void r_luaL_error(DWORD L, std::string str) {
	std::cout << "Lua Error: " << str << std::endl;
}

std::string lua_syntaxcheck(std::string code) {
	lua_State *state = luaL_newstate();
	luaL_openlibs(state);
	int result = luaL_loadstring(state, code.c_str());
	if (result == 0) {
		lua_close(state);
		return "0";
	}
	else {
		const char *str = lua_tostring(state, -1);
		lua_close(state);
		return str;
	}
}

int lua_execute(lua_State *state, std::string code) {
	int result = luaL_loadstring(state, code.c_str());
	if (result == 0) {
		int success = lua_pcall(state, 0, 0, 0);
		if (success == 0) {
			return 0;
		}
		else {
			std::cout << lua_tostring(state, -1) << std::endl;
			return 1;
		}
	}
	else {
		std::cout << lua_tostring(state, -1) << std::endl;
		return 1;
	}
}

// implementations of roblox lua C api; reconstructed from IDA (Roblox executable)

DWORD* r_index2adr(DWORD L, int idx) {
	DWORD *result;
	int v3;
	if (idx <= 0) {
		if (idx <= -10000) {
			switch (idx) {
			case -10002:
				result = (DWORD *)(L + 104);
				break;
			case -10001:
				result = (DWORD *)(L + 72);
				*result = *(DWORD *)(**(DWORD **)(*(DWORD *)(L + 12) + 16) + 12);
				result[2] = 7;
				break;
			case -10000:
				result = (DWORD *)(L + 8 + *(DWORD *)(L + 8) + 168);
				break;
			default:
				v3 = **(DWORD **)(*(DWORD *)(L + 12) + 16);
				if (-10002 - idx > *(BYTE *)(v3 + 7))
					result = NULL;
				else
					result = (DWORD *)(v3 + 16 * (-10002 - idx) + 8);
				break;
			}
		}
		else {
			result = (DWORD *)(*(DWORD *)(L + 16) + 16 * idx);
		}
	}
	else {
		result = NULL;
		if ((unsigned int)(16 * idx + *(DWORD *)(L + 28) - 16) < *(DWORD *)(L + 16))
			result = (DWORD *)(16 * idx + *(DWORD *)(L + 28) - 16);
	}
	return result;
}

static int l_strcmp(const TString *ls, const TString *rs) {
	const char *l = getstr(ls);
	size_t ll = ls->tsv.len;
	const char *r = getstr(rs);
	size_t lr = rs->tsv.len;
	for (;;) {
		int temp = strcoll(l, r);
		if (temp != 0) return temp;
		else { 
			size_t len = strlen(l);
			if (len == lr)
				return (len == ll) ? 0 : 1;
			else if (len == ll)
				return -1;
			len++;
			l += len; ll -= len; r += len; lr -= len;
		}
	}
}

TValue *r_luaV_tonumber(TValue *obj, TValue *n) {
	lua_Number num;
	if (obj->tt == R_LUA_TNUMBER) return obj;
	if (obj->tt == R_LUA_TSTRING && luaO_str2d(getstr(&(obj)->value.gc->ts), &num)) {
		setnvalue(n, num);
		return n;
	}
	else
		return NULL;
}

int r_luaV_tostring(StkId obj) {
	if (!(obj->tt == R_LUA_TNUMBER))
		return 0;
	else {
		lua_State *state = luaL_newstate();
		char s[LUAI_MAXNUMBER2STR];
		lua_Number n = obj->value.n;
		lua_number2str(s, n);
		setsvalue2s(state, obj, luaS_new(state, s));
		lua_close(state);
		return 1;
	}
}

int r_luaV_equalval(const TValue *t1, const TValue *t2) {
	switch (ttype(t1)) {
		case R_LUA_TNIL: return 1;
		case R_LUA_TNUMBER: return (t1->value.n == t2->value.n);
		case R_LUA_TBOOLEAN: return (t1->value.b == t2->value.b);
		case R_LUA_TLIGHTUSERDATA: return (t1->value.p == t2->value.p);
		case R_LUA_TUSERDATA: return (&(t1)->value.gc->u.uv == &(t2)->value.gc->u.uv);
		case R_LUA_TTABLE: return (&(t1)->value.gc->h == &(t2)->value.gc->h);
		default: return t1->value.gc == t2->value.gc;
	}
}

int r_luaV_lessthan(const TValue *t1, const TValue *t2) {
	switch (ttype(t1)) {
		case R_LUA_TNUMBER: return (t1->value.n < t2->value.n);
		case R_LUA_TSTRING: return (l_strcmp(&(t1)->value.gc->ts, &(t2)->value.gc->ts) < 0);
		default: return 0;
	}
}

void r_copyvalue(DWORD L, lua_State *L2, int idx) {
	TValue *o = (TValue *)r_index2adr(L, idx);
	switch (o->tt) {
		case R_LUA_TNIL: {
			lua_pushnil(L2);
			break;
		}
		case R_LUA_TLIGHTUSERDATA: {
			void *value = o->value.p;
			setpvalue(L2->top, value);
			api_incr_top(L2);
			break;
		}
		case R_LUA_TNUMBER: {
			double value = o->value.n;
			setnvalue(L2->top, value);
			api_incr_top(L2);
			break;
		}
		case R_LUA_TBOOLEAN: {
			int value = o->value.b;
			setbvalue(L2->top, (value != 0));
			api_incr_top(L2);
			break;
		}
		case R_LUA_TSTRING: {
			TString *value = rawtsvalue(o);
			setsvalue(L2, L2->top, value);
			api_incr_top(L2);
			break;
		}
		case R_LUA_TTHREAD: {
			lua_pushnil(L2);
			break;
		}
		case R_LUA_TFUNCTION: {
			Closure *value = clvalue(o);
			setclvalue(L2, L2->top, value);
			api_incr_top(L2);
			break;
		}
		case R_LUA_TTABLE: {
			Table *value = hvalue(o);
			sethvalue(L2, L2->top, value);
			api_incr_top(L2);
			break;
		}
		case R_LUA_TUSERDATA: {
			Udata *value = rawuvalue(o);
			setuvalue(L2, L2->top, value);
			api_incr_top(L2);
			break;
		}
	}
}

void r_copystack(DWORD L, lua_State *L2) {
	int size = (*(DWORD *)(L + 16) - *(DWORD *)(L + 28)) >> 4;
	for (int i = 1; i <= size; i++) {
		r_copyvalue(L, L2, i);
	}
}

int l2rtype(int type) {
	switch (type) {
		case LUA_TBOOLEAN:
			return R_LUA_TBOOLEAN;
		case LUA_TLIGHTUSERDATA:
			return R_LUA_TLIGHTUSERDATA;
		case LUA_TNUMBER:
			return R_LUA_TNUMBER;
		case LUA_TSTRING:
			return R_LUA_TSTRING;
		case LUA_TTABLE:
			return R_LUA_TTABLE;
		case LUA_TFUNCTION:
			return R_LUA_TFUNCTION;
		case LUA_TUSERDATA:
			return R_LUA_TUSERDATA;
		case LUA_TTHREAD:
			return R_LUA_TTHREAD;
	}
	return type;
}