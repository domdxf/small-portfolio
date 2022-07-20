#pragma once

#include <Windows.h>
#include <vector>
#include <string>

/* GLOBAL NAMESPACE */

class MemoryWriter {
	private:
		// Taken from ReturnCheck.h

		static DWORD GetFunctionSize(DWORD Address) {
			// Searches for the start of a new function, (55 8B EC) and uses that address to calculate the size of the function
			BYTE *FunctionEnd = (BYTE*)Address;

			// All function's start at bytes that align to 16 (0x10)
			do {
				FunctionEnd += 0x10;
			} while (FunctionEnd[0] != 0x55 || FunctionEnd[1] != 0x8B || FunctionEnd[2] != 0xEC);

			return (DWORD)FunctionEnd - Address;
		}
		static bool IsFunction(DWORD Address) {
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
		static void FixCalls(DWORD OriginalFunction, DWORD NewFunction, DWORD FunctionSize) {
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

							// Get the new offset, which is the function call subtracted from the caller address, and offset -5
							DWORD NewFunctionCallOffset = FunctionCall - (DWORD)Address - 5;
							*(DWORD*)(Address + 1) = NewFunctionCallOffset;

						}
					}
				}
				Address += 1;
			}
		}
	public:
		struct MemoryData {
			std::string Name;
			DWORD Address;
			BYTE *Backup;
			int Length;
		};
		static std::vector<MemoryWriter::MemoryData> Data;
		static DWORD Hook(std::string Name, BYTE *Address, DWORD Destination, int Length) {
			BYTE *Backup = new BYTE[Length + 32];
			memcpy(Backup, (void*)(Address - 16), Length + 32);
			Data.push_back({ Name, (DWORD)Address - 16, Backup, Length });

			DWORD OldProtect;
			VirtualProtect((void*)Address, Length, PAGE_EXECUTE_READWRITE, &OldProtect);

			DWORD RealAddress = (DWORD)(Destination - (DWORD)Address) - 5;
			*Address = 0xE9;
			*((DWORD*)(Address + 0x1)) = RealAddress;
			for (int i = 0x5; i < Length; i++)
				*(Address + i) = 0x90;

			VirtualProtect(Address, Length, OldProtect, NULL);

			return (DWORD)Address + Length;
		}
		static void UnHook(std::string Name) {
			for (MemoryWriter::MemoryData MemData : Data) {
				if (MemData.Name == Name) {
					DWORD OldProtect;
					VirtualProtect((void*)MemData.Address, MemData.Length, PAGE_EXECUTE_READWRITE, &OldProtect);
					memcpy((void*)(MemData.Address + 17), MemData.Backup + 17, MemData.Length);
					VirtualProtect((void*)MemData.Address, MemData.Length, OldProtect, NULL);

					delete[] MemData.Backup;
					return;
				}
			}
		}
		static DWORD CopyFunction(DWORD Address) {
			DWORD FunctionSize = GetFunctionSize(Address);
			void *NewFunction = VirtualAlloc(NULL, FunctionSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
			if (!NewFunction) {
				return Address;
			}
			memcpy(NewFunction, (void*)Address, FunctionSize);
			FixCalls(Address, (DWORD)NewFunction, FunctionSize);
			return (DWORD)NewFunction;
		}
};