#pragma once

#include <Windows.h>
#include <iostream>
#include <map>
#include "rbx_lua.h"

extern std::map<DWORD, lua_State*> RLuaStateMap;
extern std::map<lua_State*, DWORD> LLuaStateMap;

DWORD GetStatePair(lua_State *L);
lua_State *GetStatePair(DWORD RL);

void PairStates(DWORD RL, lua_State *L);