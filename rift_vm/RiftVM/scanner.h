#pragma once

#include <Windows.h>

BOOL Compare(BYTE* data, PBYTE byte, PCHAR mask);
DWORD Scan(PBYTE byte, PCHAR mask);