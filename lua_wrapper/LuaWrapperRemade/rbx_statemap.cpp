#include "rbx_statemap.h"

std::map<DWORD, lua_State*> RLuaStateMap;
std::map<lua_State*, DWORD> LLuaStateMap;

int totalstates = 0;
int rtotalstates = 0;

DWORD GetStatePair(lua_State* L) {
	if (LLuaStateMap.find(L) == LLuaStateMap.end()) {
		totalstates++;
		std::cout << "New state!" << std::endl;
		std::cout << "Total: " << totalstates << std::endl;

		lua_getfield(L, LUA_REGISTRYINDEX, "__STATEPAIR");

		DWORD MainRL = (DWORD)lua_tonumber(L, -1);
		DWORD RL = rlua_newthread(MainRL);

		// Prevent the state from getting gc'ed
		rluaL_ref(MainRL, LUA_REGISTRYINDEX);

		// Get rid of __STATEPAIR
		lua_pop(L, 1);

		LLuaStateMap[L] = RL;
		RLuaStateMap[RL] = L;
		return RL;
	} else {
		return LLuaStateMap[L];
	}
}

lua_State *GetStatePair(DWORD RL) {
	if (RLuaStateMap.find(RL) == RLuaStateMap.end()) {
		rtotalstates++;
		std::cout << "rNew state!" << std::endl;
		std::cout << "rTotal: " << rtotalstates << std::endl;

		rlua_getfield(RL, LUA_REGISTRYINDEX, "__STATEPAIR");

		lua_State *MainL = (lua_State*)rlua_topointer(RL, -1);
		lua_State *L = lua_newthread(MainL);

		// Prevent the state from getting gc'ed
		luaL_ref(MainL, LUA_REGISTRYINDEX);

		// Get rid of __STATEPAIR
		rlua_pop(RL, 1);

		RLuaStateMap[RL] = L;
		LLuaStateMap[L] = RL;
		return L;
	} else {
		return RLuaStateMap[RL];
	}
}

void PairStates(DWORD RL, lua_State *L) {
	// Pair the states in the maps
	LLuaStateMap[L] = RL;
	RLuaStateMap[RL] = L;

	// Push state info into registry
	rlua_pushnumber(RL, RL);
	rlua_setfield(RL, LUA_REGISTRYINDEX, "__MAINSTATE");
	rlua_pushlightuserdata(RL, L);
	rlua_setfield(RL, LUA_REGISTRYINDEX, "__STATEPAIR");

	lua_pushlightuserdata(L, L);
	lua_setfield(L, LUA_REGISTRYINDEX, "__MAINSTATE");
	lua_pushnumber(L, RL);
	lua_setfield(L, LUA_REGISTRYINDEX, "__STATEPAIR");
}