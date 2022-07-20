#include "sigscanner.h"

bool Scanner::Compare(const char *Data, const char *Bytes, const char *Mask) {
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

DWORD Scanner::FindPattern(DWORD Address, DWORD Length, const char *Bytes, const char *Mask) {
	for (DWORD i = 0; i < Length; i++) {
		if (Compare((char*)(Address + i), Bytes, Mask)) {
			return (Address + i);
		}
	}
	return 0;
}

DWORD Scanner::PatternScan(const char *Bytes, const char *Mask) {
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

DWORD Scanner::SigScan(const char *Bytes, const char *Mask) {
	return FindPattern((DWORD)GetModuleHandle(NULL), 0xF00000, Bytes, Mask);
}

DWORD Scanner::SigScan(std::string Bytes) {
	std::istringstream Stream(Bytes);
	std::vector<std::string> ByteVector {std::istream_iterator<std::string>{Stream}, std::istream_iterator<std::string>{}};

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

void Scanner::QueueSigScan(std::string Name, DWORD *OutputAddress, std::string Bytes, bool EnableRetcheck) {
	Data.push_back(std::make_tuple(Name, OutputAddress, Bytes, EnableRetcheck));
}

std::vector<std::string> Scanner::Run() {
	std::vector<std::string> ErrorList = {};

	// Unsigned int to match with Data.size() type
	for (unsigned int i = 0; i < Data.size(); i++) {
		std::tuple<std::string, DWORD*, std::string, bool> SigInfo = Data[i];
		DWORD result = SigScan(std::get<2>(SigInfo));
		if (result) {
			if (std::get<3>(SigInfo)) {
				*std::get<1>(SigInfo) = RetCheck::Unprotect(result);
			} else {
				*std::get<1>(SigInfo) = result;
			}
		} else {
			ErrorList.push_back(std::get<0>(SigInfo));
		}
	}
	return ErrorList;
}

void Scanner::Clear() {
	Data.clear();
}