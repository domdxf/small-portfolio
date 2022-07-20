#pragma once

#include <Windows.h>
#include <iostream>

/* GLOBAL NAMESPACE */

namespace RetCheck {
	DWORD Unprotect(DWORD Address);
	DWORD GetFunctionSize(DWORD Address) {
		// Searches for the start of a new function, (55 8B EC) and uses that address to calculate the size of the function
		BYTE *FunctionEnd = (BYTE*)Address;

		// All function's start at bytes that align to 16 (0x10)
		do {
			FunctionEnd += 0x10;
		} while (FunctionEnd[0] != 0x55 || FunctionEnd[1] != 0x8B || FunctionEnd[2] != 0xEC);

		return (DWORD)FunctionEnd - Address;
	}
	bool IsFunction(DWORD Address) {
		// Checks for prolog (55 8B EC)
		BYTE *FunctionAddress = (BYTE*)Address;
		__try {
			if (FunctionAddress[0] == 0x55 && FunctionAddress[1] == 0x8B && FunctionAddress[2] == 0xEC) {
				return true;
			}
			return false;
		} __except (EXCEPTION_EXECUTE_HANDLER) {
			return false;
		}
	}
	bool DisableRetcheck(DWORD NewFunction, DWORD FunctionSize) {
		BYTE *Address = (BYTE*)NewFunction;
		bool RetcheckFound = false;

		// Make sure the address is within bounds
		// 25 is the number of bytes we are checking to ensure if the return check exists in the function
		while ((DWORD)Address + 25 < NewFunction + FunctionSize) {

			// Checks to see if the instructions follow this order (retcheck): jb, mov, mov, sub, cmp, jb, mov, or
			// Future reference: Some return checks have different instructions due to different registers that are used
			//	A disassembler might come in handy to detect these changes, rather than hard-coding them, which we will do for now
			//  PATCH: 3rd line is for getmetatable
			if ((Address[0] == 0x72 && Address[2] == 0xA1 && Address[7] == 0x8B && Address[10] == 0x2B && Address[12] == 0x3B && Address[18] == 0x72 && Address[20] == 0xA1 && Address[25] == 0x81) ||
				(Address[0] == 0x72 && Address[2] == 0x8B && Address[8] == 0x8B && Address[11] == 0x2B && Address[13] == 0x3B && Address[19] == 0x72 && Address[21] == 0xA1 && Address[26] == 0x81) ||
				(Address[0] == 0x72 && Address[2] == 0x8B && Address[8] == 0x81 && Address[18] == 0x81 && Address[28] == 0x89 && Address[34] == 0x8B && Address[40] == 0xC7)) {

				// Change the jb to a jmp instruction
				Address[0] = 0xEB;
				RetcheckFound = true;

				// Do not break out, as there could be more return checks in the same function
			}
			Address += 1;
		}
		return RetcheckFound;
	}
	void FixCalls(DWORD OriginalFunction, DWORD NewFunction, DWORD FunctionSize) {
		BYTE *Address = (BYTE*)NewFunction;

		while ((DWORD)Address < NewFunction + FunctionSize) {

			// Check to see if the instruction is a call instruction
			if (Address[0] == 0xE8) {

				// Get the original address of the call function
				DWORD OriginalAddress = OriginalFunction + ((DWORD)Address - NewFunction);

				// Add the original address with the offset of the call function, and 5 (the call function is offset -5 from the actual function)
				DWORD FunctionCall = OriginalAddress + *(DWORD*)(Address + 1) + 5;

				// Check if the function is aligned to 16 bytes
				if (FunctionCall % 0x10 == 0) {

					// Check if the function is actually a function, and not a false positive from an argument to a different instruction
					if (IsFunction(FunctionCall)) {

						// Unprotect the function call
						DWORD NewFunctionCall = Unprotect(FunctionCall);

						// Get the new offset, which is the function call subtracted from the caller address, and offset -5
						DWORD NewFunctionCallOffset = NewFunctionCall - (DWORD)Address - 5;
						*(DWORD*)(Address + 1) = NewFunctionCallOffset;

					}
				}
			}
			Address += 1;
		}
	}
	DWORD Unprotect(DWORD Address) {
		// Addresses are not cached for simplicity

		DWORD FunctionSize = GetFunctionSize(Address);

		void *NewFunction = VirtualAlloc(NULL, FunctionSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
		if (!NewFunction) {
			return Address;
		}

		// Copy the original function into the new one
		memcpy(NewFunction, (void*)Address, FunctionSize);

		// Attempt to disable the return check
		if (!DisableRetcheck((DWORD)NewFunction, FunctionSize)) {
			VirtualFree(NewFunction, FunctionSize, MEM_RELEASE);
			return Address;
		}

		FixCalls(Address, (DWORD)NewFunction, FunctionSize);
		return (DWORD)NewFunction;
	}
}