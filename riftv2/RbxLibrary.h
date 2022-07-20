#pragma once

#include <Windows.h>
#include <string>
#include <vector>

/* CONFIG DEFINITIONS */

#define NAME_OFF		40
#define CLASS_OFF		16
#define PARENT_OFF		52
#define CHILDREN_OFF	44
#define FE_OFF			603
#define LOCALPLR_OFF	340

/* GLOBAL NAMESPACE */

namespace RBXLib {
	BOOL GetFilteringEnabled(DWORD Instance) {
		return *(BOOL*)(Instance + FE_OFF);
	}
	std::string GetName(DWORD Instance) {
		return *(std::string*)(*(DWORD*)(Instance + NAME_OFF));
	}
	std::string GetClass(DWORD Instance) {
		return *(std::string*)(*(int(**)(void))(*(DWORD*)Instance + CLASS_OFF))();
	}
	DWORD GetParent(DWORD Instance) {
		return *(DWORD*)(Instance + PARENT_OFF);
	}
	DWORD GetLocalPlayer(DWORD Instance) {
		return *(DWORD*)(Instance + LOCALPLR_OFF);
	}
	std::vector<DWORD> GetChildren(DWORD Instance) {
		std::vector<DWORD> Children;
		DWORD Start = *(DWORD*)(Instance + CHILDREN_OFF);
		DWORD End = *(DWORD*)(Start + 4);
		for (DWORD i = *(DWORD*)Start; i != End; i += 8) {
			Children.push_back(*(DWORD*)i);
		}
		return Children;
	}
	DWORD FindFirstChild(DWORD Instance, std::string Name) {
		std::vector<DWORD> Children = GetChildren(Instance);
		for (DWORD Child : Children) {
			if (GetName(Child) == Name) {
				return Child;
			}
		}
		return 0;
	}
	DWORD FindFirstChildOfClass(DWORD Instance, std::string ClassName) {
		std::vector<DWORD> Children = GetChildren(Instance);
		for (DWORD Child : Children) {
			if (GetClass(Child) == ClassName) {
				return Child;
			}
		}
		return 0;
	}
}