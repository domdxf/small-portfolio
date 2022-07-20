#include "rbx_library.h"

BOOL RBXLib::GetFilteringEnabled(DWORD Instance) {
	return *(BOOL*)(Instance + FE_OFF);
}

std::string RBXLib::GetName(DWORD Instance) {
	return *(std::string*)(*(DWORD*)(Instance + NAME_OFF));
}

std::string RBXLib::GetClass(DWORD Instance) {
	return *(std::string*)(*(int(**)(void))(*(DWORD*)Instance + CLASS_OFF))();
}

DWORD RBXLib::GetParent(DWORD Instance) {
	return *(DWORD*)(Instance + PARENT_OFF);
}

std::vector<DWORD> RBXLib::GetChildren(DWORD Instance) {
	std::vector<DWORD> children;
	DWORD start = *(DWORD*)(Instance + CHILDREN_OFF);
	DWORD end = *(DWORD*)(start + 4);

	for (DWORD i = *(DWORD*)start; i != end; i += 8) {
		children.push_back(*(DWORD*)i);
	}
	return children;
}

DWORD RBXLib::FindFirstChild(DWORD Instance, std::string Name) {
	std::vector<DWORD> children = GetChildren(Instance);

	for (DWORD child : children) {
		if (GetName(child) == Name) {
			return child;
		}
	}
	return 0;
}

DWORD RBXLib::FindFirstChildOfClass(DWORD Instance, std::string ClassName) {
	std::vector<DWORD> children = GetChildren(Instance);

	for (DWORD child : children) {
		if (GetClass(child) == ClassName) {
			return child;
		}
	}
	return 0;
}