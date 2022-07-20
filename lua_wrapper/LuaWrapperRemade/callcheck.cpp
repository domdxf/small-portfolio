#include "callcheck.h"

DWORD CallCheck::GeneralFlag;
std::vector<DWORD> CallCheck::SpecialFlags;
std::map<DWORD, DWORD> CallCheck::FunctionFlags;

__declspec(naked) void Return1() {
	__asm {
		mov     eax, 1
		pop     edi
		pop     esi
		pop     ebx
		mov     esp, ebp
		pop     ebp
		retn
	}
}

__declspec(naked) void Return2() {
	__asm {
		pop     edi
		pop     esi
		mov     eax, 2
		pop     ebx
		mov     esp, ebp
		pop     ebp
		retn
	}
}

void CallCheck::Setup() {
	// Get one general flag, and dump the rest into special flags
	DWORD Base = (DWORD)GetModuleHandle(NULL);

	for (DWORD Address = Base + 0x1000; Address < Base + 0x301000; Address++) {
		// If the address is an int3 instruction
		if (*(BYTE*)Address == 0xCC) {
			if (!GeneralFlag) {
				GeneralFlag = Address;
			} else {
				SpecialFlags.push_back(Address);
			}
		}
	}

	AddVectoredExceptionHandler(1, [](PEXCEPTION_POINTERS info) -> LONG {
		DWORD Address = (DWORD)info->ExceptionRecord->ExceptionAddress;
		if ((Address == GeneralFlag) || (FunctionFlags.find(Address) != FunctionFlags.end())) {
			std::cout << "Exception Handler" << std::endl;

			DWORD RL = (DWORD)info->ContextRecord->Esi;	// first argument passed in esi register; can also be found at [[ebp] + 8]; *(DWORD*)(*(DWORD*)info->ContextRecord->Ebp + 8)
			rlua_CFunction Function;

			// Get the function address
			if (Address == GeneralFlag) {
				Function = (rlua_CFunction)(DWORD)rlua_tonumber(RL, lua_upvalueindex(1));
			} else {
				Function = (rlua_CFunction)FunctionFlags[Address];
			}
			
			// Make the function call
			int Result = Function(RL);

			// Do cleanup (end of function call, as the code will not re-enter luaD_precall)
			if (Result < 0) {
				info->ContextRecord->Eip = (DWORD)Return2;
			} else {
				rluaD_poscall(RL, *(DWORD*)(RL + 12) - 16 * Result);
				info->ContextRecord->Eip = (DWORD)Return1;
			}

			return EXCEPTION_CONTINUE_EXECUTION;
		} else {
			return EXCEPTION_CONTINUE_SEARCH;
		}
	});
}

void CallCheck::PushCClosure(DWORD RL, rlua_CFunction Function, int Upvalues) {
	DWORD NewFlag = SpecialFlags[0];
	SpecialFlags.erase(SpecialFlags.begin());
	FunctionFlags[NewFlag] = (DWORD)Function;

	rlua_pushcclosure(RL, NewFlag, Upvalues);
}

void CallCheck::PushCFunction(DWORD RL, rlua_CFunction Function) {
	rlua_pushnumber(RL, (DWORD)Function);
	rlua_pushcclosure(RL, GeneralFlag, 1);
}