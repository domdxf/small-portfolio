/* HEADER */

#include <Windows.h>
#include <iostream>
#include <string>
#include "MemoryReader.h"
#include "MemoryWriter.h"
#include "RbxLibrary.h"
#include "RbxLua.h"
#include "StoredData.h"
extern "C" {
	#include "lua.h"
	#include "lauxlib.h"
	#include "lualib.h"
}

EXTERN_C IMAGE_DOS_HEADER __ImageBase;

//--------------------------
// TO - DO
//--------------------------

/*
1) Fix error messages [MAJOR]
	- Multi-layer pcall/ypcall
	Ex: local a, b = pcall(function()
			local a, b = pcall(function()
				error'test'
			end)
			print(b)
			error'ummm'
		end)
		print(b)
	- Xpcall
	- Errors inside loadstring (line number)

2) Implement more custom functions
	- setthreadidentity, (for require bypass)
3) Bypasses
	- trustcheck
3) Bug test!
*/


//--------------------------
// CONFIG DEFINITIONS
//--------------------------

#define AlignAddress(x)		((DWORD)GetModuleHandle(NULL) + (x - 0x400000))
#define SC_OFF				(0x40 + 2)
#define DESF_OFF			(0x1E + 2)
#define VM_STATE			0	// 0 = local; 1 = core
#define INIT_IDENTITY		6

#define WINAPI_HOOK			AlignAddress(0x4025D2)	// search for "ds" in text; must be the call instruction itself
#define SET_IDENTITY(L, i)	*(DWORD*)(L - 32) ^= (i ^ (unsigned __int8)*(DWORD*)(L - 32)) & 31;	 // look inside the function that calls sandboxthread, then look for what accesses the second arg of that func

#define XOR_DEOBFUS2(Ptr)	((DWORD)(Ptr) ^ *(DWORD*)(Ptr))
#define XOR_DEOBFUS1(Ptr)	(*(DWORD*)(Ptr) - (DWORD)(Ptr))
#define CKEY_LS_OFF			12
#define CKEY_ALGO(LS)		(XOR_DEOBFUS2(XOR_DEOBFUS2(LS + CKEY_LS_OFF) + 24))
#define STORED_CKEY			139598625
#define STORED_SIZE			79772

// Really doesn't need to be updated...
#define UnpatchCall(x)		DWORD *Handler = (DWORD*)__readfsdword(0); \
							DWORD Function = Handler[1]; \
							Handler[1] = ((DWORD)GetModuleHandle(NULL) + (0x74E010 - 0x400000)); \
							x; \
							Handler[1] = Function
							

//--------------------------
// GLOBAL VARIABLES
//--------------------------

DWORD GlobalState = 0;
DWORD RiftState = 0;
DWORD RBX_DESERIALIZEFLAG = 0;
DWORD RBX_CALLBYTE = 0;
std::vector<MemoryWriter::MemoryData> MemoryWriter::Data;

//--------------------------
// UTILITY FUNCTIONS
//--------------------------

long long MultiplicativeInverse(long long a, long long n) {
	long long t = 0;
	long long newt = 1;

	long long r = n;
	long long newr = a;

	while (newr != 0) {
		long long q = r / newr;

		long long curt = t;
		t = newt;
		newt = curt - q * newt;

		long long curr = r;
		r = newr;
		newr = curr - q * newr;
	}

	return (t < 0) ? t + n : t;
}

DWORD GetRuntimeFunction(PCHAR ModuleName, PCHAR Function) {
	HMODULE Module = GetModuleHandleA(ModuleName);
	return (DWORD)(GetProcAddress(Module, Function));
}

//--------------------------
// DLL/GUI COMMUNICATION
//--------------------------

std::string DLLBufferName = "RIFTDLL_INPUT";
std::string GUIBufferName = "RIFTGUI_INPUT";
unsigned int BufferSize = 1024 * 1024;
std::string EndPacketString = "_RIFT_";

/*
Packet Structure;
	First byte; id indicating packet type
	Every byte after; data
	last 6 bytes '_rift_'; indicates the end of the packet has been reached
*/

void WriteGUIBuffer(std::string Packet) {
	Packet = Packet + EndPacketString;
	HANDLE FileMap = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, BufferSize, GUIBufferName.c_str());
	PVOID Buffer = MapViewOfFile(FileMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
	CopyMemory(Buffer, (void*)Packet.c_str(), Packet.length());
}

void WriteToRiftOutput(std::string text, int r, int g, int b) {
	WriteGUIBuffer("1[" + std::to_string(r) + "," + std::to_string(g) + "," + std::to_string(b) + "]" + text);
}

void ReadDLLBuffer() {
	HANDLE FileMap = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, BufferSize, DLLBufferName.c_str());
	LPCSTR Buffer = (LPCSTR)MapViewOfFile(FileMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
	while (true) {
		if (Buffer[0] != 0) {
			std::string Packet = (std::string)Buffer;
			if (Packet.substr(Packet.size() - EndPacketString.size()) == EndPacketString) {
				Packet.erase(Packet.size() - EndPacketString.size(), EndPacketString.size());
				std::string PacketData = Packet.substr(1);

				ZeroMemory((void*)Buffer, BufferSize);
				switch (Packet[0] - '0') {
					case 1: {	// Execute script
						
						lua_State *L = luaL_newstate();
						if (!luaL_loadstring(L, PacketData.c_str())) {
							lua_close(L);

							std::string Code = "script = Instance.new('LocalScript') " + PacketData + "";
							DWORD NewThread = rlua_newthread(RiftState);
							sandboxthread(NewThread);

							rlua_getfield(NewThread, -10002, "dostring");
							rlua_pushstring(NewThread, Code.c_str());

							UnpatchCall(rlua_pcall(NewThread, 1, 0, 0));

						} else {
							std::string SyntaxError = lua_tostring(L, -1);
							lua_close(L);
							WriteToRiftOutput(SyntaxError, 255, 0, 0);
						}
						break;
					}
					case 2: { 	// Flag
						if (PacketData == "EXIT") {
							// DLL unloading will be handled outside of function call.
							return;
						}
					}
					default: {
						break;
					}
				}
			}
		}
		Sleep(1);
	}
}

//--------------------------
// ROBLOX CHECK BYPASSES
//--------------------------

/* SEH Check Bypass - UNFAVORED OVER HANDLE BACKUP METHOD */

// Currently only supports pcall; can easily support resume if needed
void BypassSEHCheck() {
	DWORD rluaD_unprotected = MemoryReader::SigScan("55 8B EC 6A FF 68 ? ? ? ? 64 A1 ? ? ? ? 50 64 89 25 ? ? ? ? 83 EC 20 8B 4D 08");
	DWORD rluaD_pcall = MemoryReader::SigScan("55 8B EC 83 EC 0C 53 56 57 8B 7D 08 FF 75 10");
	if (!rluaD_unprotected || !rluaD_pcall) {
		std::cout << "FAILED TO SETUP SEHCHECK BYPASS" << std::endl;
	}
	BYTE *nluaD_unprotected = (BYTE*)MemoryWriter::CopyFunction(rluaD_unprotected);
	BYTE *nluaD_pcall = (BYTE*)MemoryWriter::CopyFunction(rluaD_pcall);

	*(BYTE*)((BYTE*)nluaD_unprotected + 0x5D) = 0xEB;
	*(BYTE*)((BYTE*)nluaD_unprotected + 0x5D + 1) = 0x11;
	*(DWORD*)(nluaD_pcall + 0x35 + 1) = ((DWORD)nluaD_unprotected - (DWORD)(nluaD_pcall + 0x35) - 5);
	*(DWORD*)((BYTE*)rlua_pcall + 0x49 + 1) = ((DWORD)nluaD_pcall - (DWORD)((BYTE*)rlua_pcall + 0x49) - 5);

	std::cout << "Address of pcall: " << (DWORD*)rlua_pcall << std::endl;
	std::cout << "Address of luaD_pcall: " << (DWORD*)nluaD_pcall << std::endl;
	std::cout << "Address of luaD_unpro: " << (DWORD*)nluaD_unprotected << std::endl;
}


/* Call Check Bypass */
DWORD GetModuleHandleAJmpBack;

int CallUpvalueFunction(DWORD RL) {
	rlua_CFunction Function = (rlua_CFunction)(DWORD)rlua_tonumber(RL, rlua_upvalueindex(1));
	return Function(RL);
}

__declspec(naked) void GetModuleHandleAHook() {
	__asm {
		mov eax, [ebp + 4]			// Get the immediate return address
		movzx ecx, byte ptr[eax]	// Get the first byte of the instruction at the return address
		cmp ecx, 0x83				// Is it equal to 83 (luaD_precall)
		jne ValidCall				// If not, jump to the real call
		mov ecx, [esp + 8]			// Move lua state into ecx register
		push ecx					// Push the lua state onto the stack
		call CallUpvalueFunction	// Call `CallUpvalueFunction` which will get the cclosure from the upvalue and call it
		add esp, 4					// Stack cleanup; 1 arg * 4 = 4
		pop ecx						// Pop the return address to the winapi caller function
		ret							// Return directly to luaD_precall
	ValidCall:						// Real call; use instructions of regular GetModuleHandleA
		mov edi, edi
		push ebp
		mov ebp, esp
		pop ebp
		jmp [GetModuleHandleAJmpBack]
	};
}

void rlua_pushcfunction(DWORD RL, rlua_CFunction Function) {
	rlua_pushnumber(RL, (DWORD)Function);
	rlua_pushcclosure(RL, WINAPI_HOOK, 1);
}

//--------------------------
// ROBLOX FUNCTIONS
//--------------------------

enum MessageType {
	MESSAGE_OUTPUT,
	MESSAGE_INFO,
	MESSAGE_WARNING,
	MESSAGE_ERROR,
	MESSAGE_SENSITIVE,
	MESSAGE_TYPE_MAX
};

void DevConsoleError(const char *Text) {
	printdevconsole(MESSAGE_ERROR, Text);
}

void DevConsoleOutput(const char *Text) {
	printdevconsole(MESSAGE_OUTPUT, Text);
}

void DevConsoleWarn(const char *Text) {
	printdevconsole(MESSAGE_WARNING, Text);
}

void DevConsoleInfo(const char *Text) {
	printdevconsole(MESSAGE_INFO, Text);
}

//--------------------------
// ROBLOX IMPLEMENTATIONS
//--------------------------

// Only supports up to 5 (when would you have more args than 5 anyways?)
std::string GetNumberEquivalent(int Number) {
	switch (Number) {
		case 1:
			return "first";
		case 2:
			return "second";
		case 3:
			return "third";
		case 4:
			return "fourth";
		case 5:
			return "fifth";
		default:	// Should never happen
			return "?";
	}
}

void CheckArg(DWORD RL, std::string FunctionName, std::string ExpectedType, int Arg) {
	std::string GivenType = rlua_typename(RL, rlua_type(RL, Arg));
	if (GivenType != ExpectedType) {
		std::string ErrorMessage = "call to " + FunctionName + " failed: expected " + ExpectedType + " as " + GetNumberEquivalent(Arg) + " argument, got " + GivenType;
		rlua_pushstring(RL, ErrorMessage.c_str());
		rlua_error(RL);
	}
}

// No error messages; assumes all calls are correct
int sysrift_impl(DWORD RL) {
	if (rlua_gettop(RL) > 1) {
		std::string Command = rlua_tostring(RL, -2);
		std::string Text = rlua_tostring(RL, -1);
		if (Command == "deverror") {
			DevConsoleError(Text.c_str());
		} else if (Command == "devinfo") {
			DevConsoleInfo(Text.c_str());
		}
	}
	return 0;
}

int getrawmetatable_impl(DWORD RL) {
	if (!rlua_gettop(RL) || !rlua_getmetatable(RL, -1)) {
		rlua_pushnil(RL);
	}
	return 1;
}

int getgenv_impl(DWORD RL) {
	rlua_pushvalue(RiftState, -10002);
	rlua_xmove(RiftState, RL, 1);
	return 1;
}

int getrenv_impl(DWORD RL) {
	rlua_pushvalue(GlobalState, -10002);
	rlua_xmove(GlobalState, RL, 1);
	return 1;
}

int setreadonly_impl(DWORD RL) {
	rlua_settop(RL, 2);
	CheckArg(RL, "setreadonly", "table", 1);
	CheckArg(RL, "setreadonly", "boolean", 2);
	std::cout << "I'm here, for some reason" << std::endl;
	return 0;
}

//--------------------------
// STARTUP FUNCTIONS
//--------------------------

/* Scanning */
int ScanInit() {
	MemoryReader Scan;
	DWORD SCFunc = 0;
	DWORD DESFlagFunc = 0;
	std::cout << "Scan commencing..." << std::endl;

	Scan.QueueSigScan("callbyte", (DWORD*)&RBX_CALLBYTE, "74 97 A1", false);
	Scan.QueueSigScan("index2adr", (DWORD*)&rindex2adr, "55 8B EC 56 8B 75 0C 85 F6 7E 1C", false);
	Scan.QueueSigScan("sandboxthread", (DWORD*)&sandboxthread, "55 8B EC 56 8B 75 08 6A 00 6A 00 56 E8 ? ? ? ? 6A 00", false);
	Scan.QueueSigScan("deserialize", (DWORD*)&deserialize, "55 8B EC 64 A1 ? ? ? ? 6A FF 68 ? ? ? ? 50 64 89 25 ? ? ? ? 81 EC ? ? ? ? 8B 4D 0C", false);
	Scan.QueueSigScan("printdevconsole", (DWORD*)&printdevconsole, "55 8B EC 6A FF 68 ? ? ? ? 64 A1 ? ? ? ? 50 64 89 25 ? ? ? ? 83 EC 30 8D 45 10", false);
	Scan.QueueSigScan("deserializeflagfunc", (DWORD*)&DESFlagFunc, "55 8B EC 6A FF 68 ? ? ? ? 64 A1 ? ? ? ? 50 64 89 25 ? ? ? ? 81 EC ? ? ? ? 83 0D", false);
	Scan.QueueSigScan("scriptcontextfunc", (DWORD*)&SCFunc, "55 8B EC 6A FF 68 ? ? ? ? 64 A1 ? ? ? ? 50 64 89 25 ? ? ? ? 83 EC 4C 53 56 8B D9 57 89 5D E0", false);

	Scan.QueueSigScan("lua_getfield", (DWORD*)&rlua_getfield, "55 8B EC 6A 01 FF 75 10", false);
	Scan.QueueSigScan("lua_setfield", (DWORD*)&rlua_setfield, "55 8B EC 83 EC 10 53 56 8B 75 08 57 FF 75 0C 56 E8 ? ? ? ? 8B 55 10 83 C4 08 8B CA 8B F8 8D 59 01 8A 01 41 84 C0 75 F9 2B CB 51 52 56 E8 ? ? ? ? 89 45 F0", true);
	Scan.QueueSigScan("lua_settable", (DWORD*)&rlua_settable, "55 8B EC 56 FF 75 0C 8B 75 08 56 E8 ? ? ? ? 8B 56 18 8D 4A F0", true);
	Scan.QueueSigScan("lua_getmetatable", (DWORD*)&rlua_getmetatable, "55 8B EC 56 FF 75 0C 8B 75 08 56 E8 ? ? ? ? 83 C4 08 8B 50 08", true);
	Scan.QueueSigScan("lua_setmetatable", (DWORD*)&rlua_setmetatable, "E8 ? ? ? ? 8B C7 83 C4 24 2B 05 ? ? ? ? 89 07 8B 45 0C 83 C7 04 74 02 89 07 50 53 E8 ? ? ? ? 6A FE 53 E8 ? ? ? ? 6A FC 53 E8 ? ? ? ? 83 C4 18 5F 5E 6A FE 53 E8 ? ? ? ? 83 C4 08 5B 5D C3", false);
	Scan.QueueSigScan("lua_createtable", (DWORD*)&rlua_createtable, "55 8B EC 56 57 8B 7D 08 8B 47 14 8D 57", true);
	Scan.QueueSigScan("lua_newthread", (DWORD*)&rlua_newthread, "55 8B EC 6A FF 68 ? ? ? ? 64 A1 ? ? ? ? 50 64 89 25 ? ? ? ? 51 56 8B 75 08 8B 46 18", true);
	Scan.QueueSigScan("lua_newuserdata", (DWORD*)&rlua_newuserdata, "55 8B EC 56 8B 75 08 8B 46 14 8D 56 14 8B 0A 33 C2 33 CA 57", true);
	Scan.QueueSigScan("lua_pushcclosure", (DWORD*)&rlua_pushcclosure, "55 8B EC 53 56 8B 75 08 8B 46 14", true);
	Scan.QueueSigScan("lua_pushstring", (DWORD*)&rlua_pushstring, "55 8B EC 8B 55 0C 85 D2 75 0D", false);
	Scan.QueueSigScan("lua_pushnumber", (DWORD*)&rlua_pushnumber, "55 8B EC 8B 4D 08 0F 28 15", true);
	Scan.QueueSigScan("lua_tolstring", (DWORD*)&rlua_tolstring, "55 8B EC 56 FF 75 0C 8B 75 08 56 E8 ? ? ? ? 8B D0", true);
	Scan.QueueSigScan("lua_topointer", (DWORD*)&rlua_topointer, "55 8B EC FF 75 0C FF 75 08 E8 ? ? ? ? 83 C4 08 8B 48 08 49", false);
	Scan.QueueSigScan("lua_tonumber", (DWORD*)&rlua_tonumber, "55 8B EC 83 EC 10 FF 75 0C FF 75 08 E8", false);
	Scan.QueueSigScan("lua_pcall", (DWORD*)&rlua_pcall, "55 8B EC 8B 45 14 83 EC 08 57", true);
	Scan.QueueSigScan("lua_resume", (DWORD*)&rlua_resume, "55 8B EC 83 EC 08 56 8B 75 08 8A 46 06", true);
	Scan.QueueSigScan("lua_next", (DWORD*)&rlua_next, "55 8B EC 56 FF 75 0C 8B 75 08 56 E8 ? ? ? ? 8B 4E 18 83 E9 10 51 FF 30 56", true);
	Scan.QueueSigScan("lua_replace", (DWORD*)&rlua_replace, "55 8B EC 56 8B 75 08 57 8B 7D 0C 81 FF", true);
	Scan.QueueSigScan("lua_error", (DWORD*)&rlua_error, "55 8B EC FF 75 08 E8 ? ? ? ? A1", true);

	// Fall back to call sigs if the main ones break; no call sig for script context func (not unique)
	Scan.QueueCallSig("index2adr", (DWORD*)&rindex2adr, "E8 ? ? ? ? 83 C4 08 8B 48 08 83 F9 02 74 45 83 F9 04 75 3C 8B 00 8D 4D F4", false);
	Scan.QueueCallSig("sandboxthread", (DWORD*)&sandboxthread, "E8 ? ? ? ? 83 C4 04 8D 4E E0", false);
	Scan.QueueCallSig("deserialize", (DWORD*)&deserialize, "E8 ? ? ? ? 83 C4 10 8B 4D F4 64 89 0D ? ? ? ? 5F 5E 5B 8B E5 5D C3", false);
	Scan.QueueCallSig("printdevconsole", (DWORD*)&printdevconsole, "E8 ? ? ? ? 83 C4 0C 8D 45 E0 50", false);
	Scan.QueueCallSig("deserializeflagfunc", (DWORD*)&DESFlagFunc, "E8 ? ? ? ? 83 C4 08 8B F0 E9 ? ? ? ? 8B 4D CC", false);

	Scan.QueueCallSig("lua_getfield", (DWORD*)&rlua_getfield, "E8 ? ? ? ? BE ? ? ? ? 83 C4 10", true);
	Scan.QueueCallSig("lua_setfield", (DWORD*)&rlua_setfield, "E8 ? ? ? ? 83 C4 0C 80 7E 0D 00 75 3F", true);
	Scan.QueueCallSig("lua_settable", (DWORD*)&rlua_settable, "E8 ? ? ? ? 83 C4 18 80 7E 0D 00 75 3F", true);
	Scan.QueueCallSig("lua_getmetatable", (DWORD*)&rlua_getmetatable, "E8 ? ? ? ? 83 C4 08 85 C0 75 11 56", true);
	Scan.QueueCallSig("lua_setmetatable", (DWORD*)&rlua_setmetatable, "E8 ? ? ? ? 68 ? ? ? ? 56 E8 ? ? ? ? 83 C4 18 5E", true);
	Scan.QueueCallSig("lua_createtable", (DWORD*)&rlua_createtable, "E8 ? ? ? ? 57 8D 45 FC", true);
	Scan.QueueCallSig("lua_newthread", (DWORD*)&rlua_newthread, "E8 ? ? ? ? 8B F0 F0 FF 05 ? ? ? ? 6A 10 C6 45 FC 06", true);
	Scan.QueueCallSig("lua_newuserdata", (DWORD*)&rlua_newuserdata, "E8 ? ? ? ? 83 C4 10 B9 ? ? ? ? C6 40 EE 00", true);
	Scan.QueueCallSig("lua_pushcclosure", (DWORD*)&rlua_pushcclosure, "E8 ? ? ? ? 68 ? ? ? ? 6A FE 56 E8 ? ? ? ? 6A 00 68 ? ? ? ? 56 E8 ? ? ? ? 68", true);
	Scan.QueueCallSig("lua_pushstring", (DWORD*)&rlua_pushstring, "E8 ? ? ? ? 8B 45 08 83 C4 08 85 C0 74 05 8B 78 08", false);
	Scan.QueueCallSig("lua_pushnumber", (DWORD*)&rlua_pushnumber, " E8 ? ? ? ? 83 C4 0C B8 ? ? ? ? 5E 5F", true);
	Scan.QueueCallSig("lua_tolstring", (DWORD*)&rlua_tolstring, "E8 ? ? ? ? 8B 75 08 83 C4 0C 8B 4D 18", true);
	Scan.QueueCallSig("lua_topointer", (DWORD*)&rlua_topointer, "E8 ? ? ? ? 83 C4 08 89 45 DC 85 C0 74 30", true);
	Scan.QueueCallSig("lua_tonumber", (DWORD*)&rlua_tonumber, "E8 ? ? ? ? DD 5D F8 F2 0F 10 45 ? 83 C4 08 66 0F 2F 05", true);
	Scan.QueueCallSig("lua_pcall", (DWORD*)&rlua_pcall, "E8 ? ? ? ? 33 C9 85 C0 0F 94 C1", true);
	Scan.QueueCallSig("lua_resume", (DWORD*)&rlua_resume, "E8 ? ? ? ? 83 C4 14 85 C0 74 19 83 F8 01", true);
	Scan.QueueCallSig("lua_next", (DWORD*)&rlua_next, "E8 ? ? ? ? 83 C4 10 85 C0 75 E8", true);
	Scan.QueueCallSig("lua_replace", (DWORD*)&rlua_replace, "E8 ? ? ? ? 83 C4 10 8B 45 BC", true);
	Scan.QueueCallSig("lua_error", (DWORD*)&rlua_error, "E8 ? ? ? ? 83 C4 04 6A FF 6A 00 57", true);

	std::vector<std::string> Results = Scan.Run();
	std::cout << Results.size() << " FAILED SCANS." << std::endl;

	if (!Results.size()) {
		RBX_DESERIALIZEFLAG = *(DWORD*)(DESFlagFunc + DESF_OFF);
		DWORD ScriptContextVF = *(DWORD*)(SCFunc + SC_OFF);
		DWORD ScriptContext = Scan.PatternScan((const char*)&ScriptContextVF, "xxxx");
		GlobalState = XOR_DEOBFUS1(ScriptContext + 56 * VM_STATE + 164);
		return TRUE;
	} else {
		for (std::string Object : Results) {
			std::cout << "> " << Object << std::endl;
		}
		return FALSE;
	}
}


/* Deferred entry point */
void Main() {
	DWORD OldProtection;
	VirtualProtect(&FreeConsole, 1, PAGE_EXECUTE_READWRITE, &OldProtection);
	*(BYTE*)&FreeConsole = 0xC3;
	VirtualProtect(&FreeConsole, 1, OldProtection, NULL);

	AllocConsole();
	freopen("CONIN$", "r", stdin);
	freopen("CONOUT$", "w", stdout);
	SetConsoleTitleA("RIFT REWRITE");

	if (!ScanInit())
		return;

	std::cout << "-/\\-DEBUG INFO-/\\-" << std::endl;
	std::cout << "Base: " << (DWORD*)GetModuleHandle(NULL) << std::endl;
	std::cout << "Luastate: " << (DWORD*)GlobalState << std::endl;
	std::cout << "-/\\------------/\\-" << std::endl;

	/* BEGIN BYPASSES */
	// BypassSEHCheck();
	GetModuleHandleAJmpBack = MemoryWriter::Hook("CallCheck", (BYTE*)GetRuntimeFunction("kernel32.dll", "GetModuleHandleA"), (DWORD)GetModuleHandleAHook, 6);
	/* END BYPASSES */

	RiftState = rlua_newthread(GlobalState);
	SET_IDENTITY(RiftState, INIT_IDENTITY);

	/* Set up environment */

	// Global environment
	rlua_pushvalue(RiftState, -10002);	// globals for all local scripts
	rlua_shallowcopytable(RiftState, -1);
	rlua_replace(RiftState, -10002);

	// rlua_pushvalue(RiftState, -10002);
	// sandboxthread(RiftState);
	// rlua_setfield(RiftState, -10002, "RiftGlobals");
	
	rlua_register(RiftState, "sysrift", sysrift_impl);
	rlua_register(RiftState, "getrawmetatable", getrawmetatable_impl);
	rlua_register(RiftState, "getgenv", getgenv_impl);
	rlua_register(RiftState, "getrenv", getrenv_impl);
	rlua_register(RiftState, "setreadonly", setreadonly_impl);

	/* Init data */

	unsigned int CKey = (unsigned int)CKEY_ALGO(GlobalState);
	unsigned int DecodeKey = MultiplicativeInverse(CKey, 1ll << 32);
	unsigned int DesKey = STORED_CKEY * DecodeKey;

	std::string InitData = std::string(reinterpret_cast<const char*>(COREDATA), STORED_SIZE);
	deserialize(RiftState, InitData, "rift", DesKey);
	*(int*)RBX_DESERIALIZEFLAG = 8;
	UnpatchCall(rlua_pcall(RiftState, 0, 0, 0));
	
	WriteToRiftOutput("Connected!", 0, 128, 0);
	ReadDLLBuffer();

	/* UNLOAD THE DLL */
	CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)FreeLibrary, &__ImageBase, 0, NULL);
}

/* DLL entry point */
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
	if (fdwReason == DLL_PROCESS_ATTACH) {
		DisableThreadLibraryCalls(hinstDLL);
		CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)Main, NULL, NULL, NULL);
	}
	return TRUE;
}