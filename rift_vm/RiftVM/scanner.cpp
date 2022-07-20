#include "Scanner.h"

BOOL Compare(BYTE* data, PBYTE byte, PCHAR mask) {
	for (; *mask; ++mask, ++data, ++byte)
		__try {
		if (*mask == 'x' && *data != *byte)
			return false;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
	return (*mask) == NULL;
}

DWORD Scan(PBYTE byte, PCHAR mask) {
	MEMORY_BASIC_INFORMATION mbi;
	PCHAR addr = 0;
	while (sizeof(mbi) == VirtualQuery(addr, &mbi, sizeof(mbi))) {
		addr += mbi.RegionSize;
		if (mbi.RegionSize < strlen(mask)) continue;
		if (mbi.State != MEM_COMMIT) continue;
		if (mbi.Type != MEM_PRIVATE) continue;
		if (mbi.AllocationProtect != PAGE_READWRITE) continue;
		if (mbi.Protect & (PAGE_GUARD | PAGE_NOCACHE | PAGE_NOACCESS)) continue;
		mbi.RegionSize -= strlen(mask);
		for (DWORD i = (DWORD)mbi.BaseAddress; i < (DWORD)mbi.BaseAddress + mbi.RegionSize; i++) {
			if (Compare((BYTE*)(i), byte, mask)) {
				return(i);
			}
		}
	}
	return 0;
}