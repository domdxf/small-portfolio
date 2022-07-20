#pragma once

#include <Windows.h>
#include <iostream>
#include <string>
#include <map>
#include "rbx_lua.h"
#include "rbx_statemap.h"
#include "callcheck.h"

/*
	OriginFunctionRefs; are used to keep track of whether or not a given native function has been ref'd or not

	ProxyFunctionRefs; are used to keep track of what ref a proxy function points to, so that when the proxy function
					   is unwrapped, the ref can just be grabbed, and a proxy to the proxy function is not created
*/


// Real Function : Function Ref
// Functions native to roblox, which were ref'd so that a copy of them is stored in the registry
extern std::map<DWORD*, int> ROriginFunctionRefs;

// Proxy Function : Real Function Ref
// Functions created by roblox, to be used in vanilla lua, that point to the real roblox function ref they will call
extern std::map<DWORD*, int> RProxyFunctionRefs;

//---

// Real Function : Function Ref
// Functions native to vanilla lua, which were ref'd so that a copy of them is stored in the registry
extern std::map<DWORD*, int> LOriginFunctionRefs;

// Proxy Function : Real Function Ref
// Functions created by vanilla lua, to be used in roblox, that point to the real vanilla lua function ref they will call
extern std::map<DWORD*, int> LProxyFunctionRefs;


//---

// Represents a roblox userdata on the lua side;
struct UserdataRef {
	int Ref;	// The ref id of the userid that is stored in the registry
};

//--

// Real Userdata : Function Userdata
// Userdatas native to roblox, which were ref'd so that a copy of them is stored in the registry
extern std::map<DWORD*, int> ROriginUserdataRefs;

// Proxy Userdata : Real Userdata Ref
// Userdatas created by roblox, to be used in vanilla lua, that point to the real roblox userdataref struct they will use
extern std::map<DWORD*, UserdataRef*> RProxyUserdataRefs;

//---

// Real Userdata : Userdata Ref
// Userdatas native to vanilla lua, which were ref'd so that a copy of them is stored in the registry
extern std::map<DWORD*, int> LOriginUserdataRefs;

// Proxy Userdata : Real Userdata Ref
// Userdatas created by vanilla lua, to be used in roblox, that point to the real vanilla lua userdataref struct they will use
extern std::map<DWORD*, UserdataRef*> LProxyUserdataRefs;


// R -> L
void Wrap(DWORD RL, lua_State *L, int Index);

// L -> R
void Unwrap(DWORD RL, lua_State *L, int Index);

int CallMetamethod(lua_State *L, std::string Metamethod);

int __UIndex(lua_State* L);
int __UNewIndex(lua_State* L);
int __UToString(lua_State* L);
int __UAdd(lua_State* L);
int __USub(lua_State* L);
int __UMul(lua_State* L);
int __UDiv(lua_State* L);
int __UUnm(lua_State* L);