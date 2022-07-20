#include "rbx_wrap.h"

std::map<DWORD*, int> ROriginFunctionRefs;
std::map<DWORD*, int> RProxyFunctionRefs;

std::map<DWORD*, int> LOriginFunctionRefs;
std::map<DWORD*, int> LProxyFunctionRefs;

std::map<DWORD*, int> ROriginUserdataRefs;
std::map<DWORD*, UserdataRef*> RProxyUserdataRefs;

std::map<DWORD*, int> LOriginUserdataRefs;
std::map<DWORD*, UserdataRef*> LProxyUserdataRefs;


int ResumeFromYield(DWORD RL) {
	lua_State *L = GetStatePair(RL);
	DWORD NewRL = rlua_newthread(RLuaState);

	PairStates(NewRL, L);

	int RArgs = rlua_gettop(RL);	
	for (int x = RArgs; x != 0; x--) {
		Wrap(RL, L, -(x));
	}
	rlua_pop(RL, RArgs);

	int Result = lua_resume(L, RArgs);

	// Just overwrite the variable for testing purposes.
	return Result;
}

// Represents a roblox function on the vanilla lua state
int LuaFunctionRef(lua_State *L) {
	DWORD RL = GetStatePair(L);

	int Ref = lua_tonumber(L, lua_upvalueindex(1));
	rlua_rawgeti(RL, LUA_REGISTRYINDEX, Ref);

	int Args = lua_gettop(L);
	for (int x = Args; x != 0; x--) {
		Unwrap(RL, L, -(x));
	}
	lua_pop(L, Args);

	// Make the call
	int Result = rlua_pcall(RL, Args, LUA_MULTRET, 0);

	if (Result) {
		std::string Error = rlua_tostring(RL, -1);
		rlua_pop(RL, 1);

		if (Error == "attempt to yield across metamethod/C-call boundary") {
			std::cout << "YIELDING" << std::endl;
			CallCheck::PushCFunction(RL, ResumeFromYield);
			return lua_yield(L, 0);
		} else {
			std::cout << "Error on roblox stack: " << Error << std::endl;
		}

		// TODO: Handle errors -- HERE --

		return 0;
	} else {
		int RArgs = rlua_gettop(RL);
		for (int x = RArgs; x != 0; x--) {
			Wrap(RL, L, -(x));
		}
		rlua_pop(RL, RArgs);

		return RArgs;
	}	
}

int RLuaFunctionRef(DWORD RL) {
	lua_State *L = GetStatePair(RL);
	DWORD NewRL = rlua_newthread(RLuaState);

	PairStates(NewRL, L);

	int Ref = rlua_tonumber(RL, lua_upvalueindex(1));
	lua_rawgeti(L, LUA_REGISTRYINDEX, Ref);

	int RArgs = rlua_gettop(RL);
	for (int x = RArgs; x != 0; x--) {
		Wrap(RL, L, -(x));
	}
	rlua_pop(RL, RArgs);

	// Replace value for testing
	RL = NewRL;

	int Result = lua_resume(L, RArgs);
	if (Result) {
		if (Result == 1) {
			std::cout << "Thread yielded." << std::endl;
		} else {
			std::string Error = lua_tostring(L, -1);
			std::cout << "Error: " << Error << std::endl;
		}
	}

	int Args = lua_gettop(L);
	for (int x = Args; x != 0; x--) {
		Unwrap(RL, L, -(x));
	}
	lua_pop(L, Args);

	return Args;
}

void Wrap(DWORD RL, lua_State *L, int Index) {
	switch (rlua_type(RL, Index)) {
		case RLUA_TNIL: {
			lua_pushnil(L);
			break;
		}
		case RLUA_TNUMBER: {
			double Value = rlua_tonumber(RL, Index);
			lua_pushnumber(L, Value);
			break;
		}
		case RLUA_TBOOLEAN: {
			int Value = rlua_toboolean(RL, Index);
			lua_pushboolean(L, Value);
			break;
		}
		case RLUA_TSTRING: {
			const char *Value = rlua_tostring(RL, Index);
			lua_pushstring(L, Value);
			break;
		}
		case RLUA_TFUNCTION: {
			// Pushing a roblox function to the vanilla lua side

			DWORD *Function = (DWORD*)rlua_topointer(RL, Index);

			// Was the function native to vanilla lua?
			if (LProxyFunctionRefs.find(Function) != LProxyFunctionRefs.end()) {
				// Function native to vanilla lua

				lua_rawgeti(L, LUA_REGISTRYINDEX, LProxyFunctionRefs[Function]);
			} else {
				// Function is native to roblox

				int Ref;

				// Was the function ref'd before?
				if (ROriginFunctionRefs.find(Function) != ROriginFunctionRefs.end()) {
					// Ref'd before
					Ref = ROriginFunctionRefs[Function];
				} else {
					// Not ref'd before
					rlua_pushvalue(RL, Index);
					Ref = rluaL_ref(RL, LUA_REGISTRYINDEX);
					ROriginFunctionRefs[Function] = Ref;
				}

				lua_pushnumber(L, Ref);
				lua_pushcclosure(L, LuaFunctionRef, 1);

				DWORD *ProxyFunction = (DWORD*)lua_topointer(L, -1);
				RProxyFunctionRefs[ProxyFunction] = Ref;
			}

			break;
		}
		case RLUA_TTABLE: {
			// Make a copy of the table

			// Table
			lua_newtable(L);

			rlua_pushvalue(RL, Index);	// Copy of roblox table
			rlua_pushnil(RL);			// Key

			while (rlua_next(RL, -2)) {
				// value = -1, key = -2
				if (rlua_type(RL, -2) == RLUA_TSTRING) {
					const char *Key = rlua_tostring(RL, -2);
					Wrap(RL, L, -1);
					lua_setfield(L, -2, Key);
				} else {
					int Key = rlua_tonumber(RL, -2);
					Wrap(RL, L, -1);
					lua_rawseti(L, -2, Key);
				}
				// pop the value from the stack, and leave the key
				rlua_pop(RL, 1);
			}

			// remove the copy of the table that was made at the beginning
			rlua_pop(RL, 1);

			break;
		}
		case RLUA_TUSERDATA: {
			// Pushing a roblox userdata to the vanilla lua side

			DWORD *UD = (DWORD*)rlua_topointer(RL, Index);

			// Was the userdata native to vanilla lua?
			if (LProxyUserdataRefs.find(UD) != LProxyUserdataRefs.end()) {
				// Userdata native to vanilla lua
				UserdataRef *Userdata = LProxyUserdataRefs[UD];
				lua_rawgeti(L, LUA_REGISTRYINDEX, Userdata->Ref);
			} else {
				// Userdata native to roblox
				UserdataRef* Userdata = (UserdataRef*)lua_newuserdata(L, sizeof(UserdataRef));

				// Was the userdata ref'd before?
				if (ROriginUserdataRefs.find(UD) != ROriginUserdataRefs.end()) {
					// Ref'd before
					Userdata->Ref = ROriginUserdataRefs[UD];
				} else {
					// Not ref'd before
					rlua_pushvalue(RL, Index);
					Userdata->Ref = rluaL_ref(RL, LUA_REGISTRYINDEX);
					ROriginUserdataRefs[UD] = Userdata->Ref;
				}
				RProxyUserdataRefs[(DWORD*)Userdata] = Userdata;

				luaL_getmetatable(L, "ruserdata_mt");
				lua_setmetatable(L, -2);
			}
			break;
		}
		default: {
			std::cout << "Attempted to wrap a `" << rlua_typename(RL, rlua_type(RL, Index)) << "`, an unsupported type." << std::endl;
		}
	}
}

void Unwrap(DWORD RL, lua_State *L, int Index) {
	switch (lua_type(L, Index)) {
		case LUA_TNIL: {
			rlua_pushnil(RL);
			break;
		}
		case LUA_TNUMBER: {
			double Value = lua_tonumber(L, Index);
			rlua_pushnumber(RL, Value);
			break;
		}
		case LUA_TBOOLEAN: {
			int Value = lua_toboolean(L, Index);
			rlua_pushboolean(RL, Value);
			break;
		}
		case LUA_TSTRING: {
			const char *Value = lua_tostring(L, Index);
			rlua_pushstring(RL, Value);
			break;
		}
		case LUA_TFUNCTION: {
			// Pushing a vanilla lua function to the roblox side

			DWORD *Function = (DWORD*)lua_topointer(L, Index);

			// Was the function native to roblox?
			if (RProxyFunctionRefs.find(Function) != RProxyFunctionRefs.end()) {
				// Function native to roblox

				rlua_rawgeti(RL, LUA_REGISTRYINDEX, RProxyFunctionRefs[Function]);
			} else {
				// Function is native to vanilla lua

				int Ref;

				// Was the function ref'd before?
				if (LOriginFunctionRefs.find(Function) != LOriginFunctionRefs.end()) {
					// Ref'd before
					Ref = LOriginFunctionRefs[Function];
				} else {
					// Not ref'd before
					lua_pushvalue(L, Index);
					Ref = luaL_ref(L, LUA_REGISTRYINDEX);
					LOriginFunctionRefs[Function] = Ref;
				}

				rlua_pushnumber(RL, Ref);
				CallCheck::PushCClosure(RL, RLuaFunctionRef, 1);

				DWORD *ProxyFunction = (DWORD*)rlua_topointer(RL, -1);
				LProxyFunctionRefs[ProxyFunction] = Ref;
			}
			break;
		}
		case LUA_TTABLE: {
			// Make a copy of the table

			// Table
			rlua_newtable(RL);

			lua_pushvalue(L, Index);	// Copy of vanilla lua table
			lua_pushnil(L);			// Key

			while (lua_next(L, -2)) {
				// value = -1, key = -2
				if (lua_type(L, -2) == LUA_TSTRING) {
					const char *Key = lua_tostring(L, -2);
					Unwrap(RL, L, -1);
					rlua_setfield(RL, -2, Key);
				} else {
					int Key = lua_tonumber(L, -2);
					Unwrap(RL, L, -1);
					rlua_rawseti(RL, -2, Key);
				}
				// pop the value from the stack, and leave the key
				lua_pop(L, 1);
			}

			// remove the copy of the table that was made at the beginning
			lua_pop(L, 1);

			break;
		}
		case LUA_TUSERDATA: {
			// Pushing a vanilla lua userdata to the roblox side

			DWORD *UD = (DWORD*)lua_topointer(L, Index);

			// Was the function native to roblox?
			if (RProxyUserdataRefs.find(UD) != RProxyUserdataRefs.end()) {
				// Userdata native to roblox
				UserdataRef *Userdata = RProxyUserdataRefs[UD];
				rlua_rawgeti(RL, LUA_REGISTRYINDEX, Userdata->Ref);
			} else {
				// Function is native to vanilla lua
				UserdataRef* Userdata = (UserdataRef*)rlua_newuserdata(RL, sizeof(UserdataRef));

				// Was the function ref'd before?
				if (LOriginUserdataRefs.find(UD) != LOriginUserdataRefs.end()) {
					// Ref'd before
					Userdata->Ref = LOriginUserdataRefs[UD];
				} else {
					// Not ref'd before
					lua_pushvalue(L, Index);
					Userdata->Ref = luaL_ref(L, LUA_REGISTRYINDEX);
					LOriginUserdataRefs[UD] = Userdata->Ref;
				}

				LProxyUserdataRefs[(DWORD*)Userdata] = Userdata;
			}
			break;
		}
		default: {
			std::cout << "Attempted to unwrap a `" << lua_typename(L, lua_type(L, Index)) << "`, an unsupported type." << std::endl;
		}
	}
}

// Pretty much the same as LuaFunctionRef
int CallMetamethod(lua_State *L, std::string Metamethod) {

	DWORD RL = GetStatePair(L);
	int Args = lua_gettop(L);
	// Unwrap the userdata and the metamethod function
	Unwrap(RL, L, -(Args));
	rluaL_getmetafield(RL, -1, Metamethod.c_str());
	if (rlua_type(RL, -1) != RLUA_TFUNCTION) {
		std::cout << "Failed to get the metamethod: " << Metamethod << std::endl;
	}

	// Remove the userdata
	rlua_remove(RL, -2);

	// Copy over all the args for the call
	for (int x = Args; x != 0; x--) {
		Unwrap(RL, L, -(x));
	}
	lua_pop(L, Args);

	// Make the call
	int Result = rlua_pcall(RL, Args, LUA_MULTRET, 0);

	if (Result) {
		std::string Error = rlua_tostring(RL, -1);
		rlua_pop(RL, 1);

		if (Error == "attempt to yield across metamethod/C-call boundary") {
			std::cout << "YIELDING" << std::endl;
			CallCheck::PushCFunction(RL, ResumeFromYield);
			return lua_yield(L, 0);
		} else {
			std::cout << "Error on roblox stack: " << Error << std::endl;
		}

		// TODO: Handle errors -- HERE --

		return 0;
	} else {
		int RArgs = rlua_gettop(RL);
		for (int x = RArgs; x != 0; x--) {
			Wrap(RL, L, -(x));
		}
		rlua_pop(RL, RArgs);
		return RArgs;
	}
}

// Metamethods
int __UIndex(lua_State* L) {
	return CallMetamethod(L, "__index");
}
int __UNewIndex(lua_State* L) {
	return CallMetamethod(L, "__newindex");
}
int __UToString(lua_State* L) {
	return CallMetamethod(L, "__tostring");
}
int __UAdd(lua_State* L) {
	return CallMetamethod(L, "__add");
}
int __USub(lua_State* L) {
	return CallMetamethod(L, "__sub");
}
int __UMul(lua_State* L) {
	return CallMetamethod(L, "__mul");
}
int __UDiv(lua_State* L) {
	return CallMetamethod(L, "__div");
}
int __UUnm(lua_State* L) {
	return CallMetamethod(L, "__unm");
}