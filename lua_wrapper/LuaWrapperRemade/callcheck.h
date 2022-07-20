#pragma once

#include <Windows.h>
#include <iostream>
#include <vector>
#include <map>
#include "rbx_lua.h"

// Due to the nature of the callcheck bypass, the structure of the code will utilize namespaces
namespace CallCheck {
	extern DWORD GeneralFlag;
	extern std::vector<DWORD> SpecialFlags;
	extern std::map<DWORD, DWORD> FunctionFlags;

	void Setup();
	void PushCClosure(DWORD RL, rlua_CFunction Function, int Upvalues);	// Utilizes special flags
	void PushCFunction(DWORD RL, rlua_CFunction Function);				// Utilizes the general flag
}