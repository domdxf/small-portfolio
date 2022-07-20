#define _CRT_SECURE_NO_WARNINGS

#include <Windows.h>
#include <iostream>
#include <TlHelp32.h>
#include <comdef.h>
#include <string>

DWORD GetProcessId(const char *process) {
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
	if (snapshot != INVALID_HANDLE_VALUE) {
		PROCESSENTRY32 info;
		info.dwSize = sizeof(info);
		if (Process32First(snapshot, &info)) {
			do {
				if (_stricmp(_bstr_t(info.szExeFile), _bstr_t(process)) == 0) {
					return info.th32ProcessID;
				}
			} while (Process32Next(snapshot, &info));
		}
	}
	return NULL;
}

bool InjectDLL(std::string ProcessName, std::string DLLName) {
	char Buffer[MAX_PATH];
	if (GetFullPathNameA(DLLName.c_str(), MAX_PATH, Buffer, NULL)) {
		std::string Path = Buffer;
		DWORD PID = GetProcessId(ProcessName.c_str());
		if (PID) {
			HMODULE Kernel32 = GetModuleHandleA("Kernel32.dll");
			if (Kernel32) {
				FARPROC LoadLib = GetProcAddress(Kernel32, "LoadLibraryA");
				if (LoadLib) {
					HANDLE Process = OpenProcess(PROCESS_ALL_ACCESS, FALSE, PID);
					if (Process) {
						LPVOID Alloc = VirtualAllocEx(Process, NULL, Path.size(), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
						if (Alloc) {
							if (WriteProcessMemory(Process, Alloc, Path.c_str(), Path.size(), NULL)) {
								CreateRemoteThread(Process, NULL, NULL, (LPTHREAD_START_ROUTINE)LoadLib, Alloc, NULL, NULL);
								CloseHandle(Process);
								return true;
							}
						}
					}
				}
			}
		}
	}
	return false;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow) {
	AllocConsole();
	SetConsoleTitleA("Injector");
	freopen("CONIN$", "r", stdin);
	freopen("CONOUT$", "w", stdout);

	bool result = InjectDLL("RobloxPlayerBeta.exe", "LuaWrapperRemade.dll");

	if (result) {
		std::cout << "Loaded!" << std::endl;
	} else {
		std::cout << "Failed!" << std::endl;
	}

	Sleep(3000);
}