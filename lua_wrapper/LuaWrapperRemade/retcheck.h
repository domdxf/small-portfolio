#pragma once

#include <Windows.h>
#include <iostream>

namespace RetCheck {
	DWORD GetFunctionSize(DWORD Address);
	bool IsFunction(DWORD Address);
	bool DisableRetcheck(DWORD NewFunction, DWORD FunctionSize);
	void FixCalls(DWORD OriginalFunction, DWORD NewFunction, DWORD FunctionSize);

	DWORD Unprotect(DWORD Address);
}