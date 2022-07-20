#pragma once

#include <Windows.h>
#include <iostream>
#include <sstream>
#include <iterator>
#include <vector>
#include <tuple>
#include "ReturnCheck.h"

/* GLOBAL NAMESPACE */

class MemoryReader {
	private:
		std::vector<std::tuple<std::string, std::string, DWORD*, std::string, bool>> Data;
		static bool Compare(const char *Data, const char *Bytes, const char *Mask) {
			for (; *Mask; ++Mask, ++Data, ++Bytes) {
				__try {
					if (*Mask == 'x' && *Data != *Bytes) {
						return false;
					}
				} __except (EXCEPTION_EXECUTE_HANDLER) {
					return false;
				}
			}
			return (*Mask) == NULL;
		}
		static DWORD FindPattern(DWORD Address, DWORD Length, const char *Bytes, const char *Mask) {
			for (DWORD i = 0; i < Length; i++) {
				if (Compare((char*)(Address + i), Bytes, Mask)) {
					return (Address + i);
				}
			}
			return 0;
		}
	public:
		static DWORD PatternScan(const char *Bytes, const char *Mask) {
			MEMORY_BASIC_INFORMATION mbi;
			PCHAR addr = 0;
			while (sizeof(mbi) == VirtualQuery(addr, &mbi, sizeof(mbi))) {
				addr += mbi.RegionSize;
				if (mbi.RegionSize < strlen(Mask)) continue;
				if (mbi.State != MEM_COMMIT) continue;
				if (mbi.Type != MEM_PRIVATE) continue;
				if (mbi.AllocationProtect != PAGE_READWRITE) continue;
				if (mbi.Protect & (PAGE_GUARD | PAGE_NOCACHE | PAGE_NOACCESS)) continue;
				mbi.RegionSize -= strlen(Mask);
				for (DWORD i = (DWORD)mbi.BaseAddress; i < (DWORD)mbi.BaseAddress + mbi.RegionSize; i++) {
					if (Compare((char*)(i), Bytes, Mask)) {
						return i;
					}
				}
			}
			return 0;
		}
		static DWORD SigScan(const char *Bytes, const char *Mask) {
			return FindPattern((DWORD)GetModuleHandle(NULL), 0xF00000, Bytes, Mask);
		}
		static DWORD SigScan(std::string Bytes) {
			std::istringstream Stream(Bytes);
			std::vector<std::string> ByteVector{ std::istream_iterator<std::string>{Stream}, std::istream_iterator<std::string>{} };

			std::string NewBytes = "";
			std::string Mask = "";

			for (std::string Byte : ByteVector) {
				if (Byte == "?" || Byte == "??") {
					NewBytes = NewBytes + (char)strtoul("0x99", (char**)0, 0);
					Mask = Mask + "?";
				} else {
					NewBytes = NewBytes + (char)strtoul(("0x" + Byte).c_str(), (char**)0, 0);
					Mask = Mask + "x";
				}
			}

			return SigScan(NewBytes.c_str(), Mask.c_str());
		}
		static DWORD GrabFromCall(DWORD Address) {
			return (Address + *(DWORD*)(Address + 1) + 5);
		}
		static DWORD GrabFromCallSig(std::string Bytes) {
			return GrabFromCall(SigScan(Bytes));
		}

		void QueueSigScan(std::string Name, DWORD *OutputAddress, std::string Bytes, bool EnableRetcheck) {
			Data.push_back(std::make_tuple("SigScan", Name, OutputAddress, Bytes, EnableRetcheck));
		}
		void QueueCallSig(std::string Name, DWORD *OutputAddress, std::string Bytes, bool EnableRetcheck) {
			Data.push_back(std::make_tuple("CallSig", Name, OutputAddress, Bytes, EnableRetcheck));
		}
		std::vector<std::string> Run() {
			std::vector<std::string> ErrorList = {};
			for (unsigned int i = 0; i < Data.size(); i++) {
				std::tuple<std::string, std::string, DWORD*, std::string, bool> SigInfo = Data[i];
				if (!*std::get<2>(SigInfo)) {
					DWORD Result = 0;
					if (std::get<0>(SigInfo) == "SigScan") {
						Result = SigScan(std::get<3>(SigInfo));
					} else {
						Result = GrabFromCallSig(std::get<3>(SigInfo));
					}
					if (Result) {
						if (std::get<0>(SigInfo) == "CallSig") {
							std::cout << "DEFAULTING TO CALL SIG FOR: " << std::get<1>(SigInfo) << std::endl;
						}
						if (std::get<4>(SigInfo)) {
							*std::get<2>(SigInfo) = RetCheck::Unprotect(Result);
							if (*std::get<2>(SigInfo) == Result) {
								std::cout << "Failed to remove retcheck for: " << std::get<1>(SigInfo) << std::endl;
							}
						} else {
							*std::get<2>(SigInfo) = Result;
						}						
						for (unsigned int i = 0; i < ErrorList.size(); i++) {
							if (ErrorList[i] == std::get<1>(SigInfo)) {
								ErrorList.erase(ErrorList.begin() + i);
							}
						}
					} else {
						ErrorList.push_back(std::get<1>(SigInfo));
					}
				}
			}
			return ErrorList;
		}
		void Clear() {
			Data.clear();
		}
};