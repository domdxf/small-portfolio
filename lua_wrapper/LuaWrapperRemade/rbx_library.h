#pragma once

#include <Windows.h>
#include <string>
#include <vector>

#define NAME_OFF		40
#define CLASS_OFF		16
#define PARENT_OFF		52
#define CHILDREN_OFF	44
#define FE_OFF			603

namespace RBXLib {
	BOOL GetFilteringEnabled(DWORD Instance);
	std::string GetName(DWORD Instance);
	std::string GetClass(DWORD Instance);
	DWORD GetParent(DWORD Instance);
	std::vector<DWORD> GetChildren(DWORD Instance);

	DWORD FindFirstChild(DWORD Instance, std::string Name);
	DWORD FindFirstChildOfClass(DWORD Instance, std::string ClassName);
}