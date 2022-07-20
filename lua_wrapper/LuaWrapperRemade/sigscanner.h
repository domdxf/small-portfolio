#pragma once

#include <Windows.h>
#include <iostream>
#include <sstream>
#include <iterator>
#include <vector>
#include <tuple>
#include "retcheck.h"

class Scanner {
	std::vector<std::tuple<std::string, DWORD*, std::string, bool>> Data;
	static bool Compare(const char *Data, const char *Bytes, const char *Mask);
	static DWORD FindPattern(DWORD Address, DWORD Length, const char *Bytes, const char *Mask);
public:
	static DWORD PatternScan(const char *Bytes, const char *Mask);
	static DWORD SigScan(const char *Bytes, const char *Mask);
	static DWORD SigScan(std::string Bytes);
	void QueueSigScan(std::string Name, DWORD *OutputAddress, std::string Bytes, bool EnableRetcheck);
	std::vector<std::string> Run();
	void Clear();
};