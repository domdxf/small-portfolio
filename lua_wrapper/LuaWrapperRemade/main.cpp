#include "main.h"

//--

DWORD DataModelVF;
DWORD DataModel;
DWORD ScriptContext;

//--

void SetGlobal(lua_State *L, std::string Global) {
	DWORD RL = GetStatePair(L);
	rlua_getglobal(RL, Global.c_str());
	Wrap(RL, L, -1);
	lua_setglobal(L, Global.c_str());
	rlua_pop(RL, 1);
}

void InitLuaState(lua_State *L) {
	luaL_newmetatable(L, "ruserdata_mt");
	lua_pushcfunction(L, __UIndex);
	lua_setfield(L, -2, "__index");
	lua_pushcfunction(L, __UNewIndex);
	lua_setfield(L, -2, "__newindex");
	lua_pushcfunction(L, __UToString);
	lua_setfield(L, -2, "__tostring");
	lua_pushcfunction(L, __UAdd);
	lua_setfield(L, -2, "__add");
	lua_pushcfunction(L, __USub);
	lua_setfield(L, -2, "__sub");
	lua_pushcfunction(L, __UMul);
	lua_setfield(L, -2, "__mul");
	lua_pushcfunction(L, __UDiv);
	lua_setfield(L, -2, "__div");
	lua_pushcfunction(L, __UUnm);
	lua_setfield(L, -2, "__unm");
	lua_pop(L, 1);

	SetGlobal(L, "print");
	SetGlobal(L, "warn");
	SetGlobal(L, "wait");
	SetGlobal(L, "error");
	SetGlobal(L, "spawn");
	SetGlobal(L, "delay");
	SetGlobal(L, "tick");
	SetGlobal(L, "game");
	SetGlobal(L, "Game");
	SetGlobal(L, "Workspace");
	SetGlobal(L, "workspace");
	//SetGlobal(L, "ypcall");
	//SetGlobal(L, "pcall");
	SetGlobal(L, "CFrame");
	SetGlobal(L, "UDim");
	SetGlobal(L, "NumberSequence");
	SetGlobal(L, "Color3");
	SetGlobal(L, "Enum");
	SetGlobal(L, "Stats");
	SetGlobal(L, "UserSettings");
	SetGlobal(L, "NumberRange");
	SetGlobal(L, "PhysicalProperties");
	SetGlobal(L, "PluginManager");
	SetGlobal(L, "NumberSequenceKeypoint");
	SetGlobal(L, "Vector2");
	SetGlobal(L, "printidentity");
	SetGlobal(L, "typeof");
	SetGlobal(L, "UDim2");
	SetGlobal(L, "TweenInfo");
	SetGlobal(L, "LoadLibrary");
	SetGlobal(L, "require");
	SetGlobal(L, "Vector3");
	SetGlobal(L, "Vector3int16");
	SetGlobal(L, "Region3int16");
	SetGlobal(L, "Region3");
	SetGlobal(L, "ColorSequenceKeypoint");
	SetGlobal(L, "ColorSequence");
	SetGlobal(L, "Instance");
	SetGlobal(L, "ElapsedTime");
	SetGlobal(L, "Faces");
	SetGlobal(L, "Rect");
	SetGlobal(L, "BrickColor");
	SetGlobal(L, "Axes");
}

//--

int InitScan() {
	DWORD DataModelFunc;
	Scanner scanner;

	std::cout << "Scanning..." << std::endl;
	
	scanner.QueueSigScan("lua_getfield", (DWORD*)&rlua_getfield, "55 8B EC 6A 01 FF 75 10", true);
	scanner.QueueSigScan("lua_setfield", (DWORD*)&rlua_setfield, "55 8B EC 83 EC 10 53 56 8B 75 08 57 FF 75 0C 56 E8 ? ? ? ? 8B 55 10 83 C4 08 8B CA 8B F8 8D 59 01 8A 01 41 84 C0 75 F9 2B CB 51 52 56 E8 ? ? ? ? 89 45 F0", true);
	scanner.QueueSigScan("lua_rawgeti", (DWORD*)&rlua_rawgeti, "55 8B EC 56 FF 75 0C 8B 75 08 56 E8 ? ? ? ? FF 75 10", true);
	scanner.QueueSigScan("lua_rawseti", (DWORD*)&rlua_rawseti, "55 8B EC 83 EC 14 56 FF 75 0C", true);
	scanner.QueueSigScan("lua_getmetatable", (DWORD*)&rlua_getmetatable, "55 8B EC 56 FF 75 0C 8B 75 08 56 E8 ? ? ? ? 83 C4 08 8B 50 08", true);
	scanner.QueueSigScan("lua_setmetatable", (DWORD*)&rlua_setmetatable, "55 8B EC 53 56 57 FF 75 0C 8B 7D 08", true);
	scanner.QueueSigScan("lua_newthread", (DWORD*)&rlua_newthread, "55 8B EC 6A FF 68 ? ? ? ? 64 A1 ? ? ? ? 50 64 89 25 ? ? ? ? 51 56 8B 75 08 8B 46 10", true);
	scanner.QueueSigScan("lua_newuserdata", (DWORD*)&rlua_newuserdata, "55 8B EC 56 8B 75 08 8B 46 10 8D 56 10 8B 0A 2B C2 2B CA 57", true);
	scanner.QueueSigScan("lua_pushcclosure", (DWORD*)&rlua_pushcclosure, "55 8B EC 53 56 8B 75 08 8B 46 10", true);
	scanner.QueueSigScan("lua_pushstring", (DWORD*)&rlua_pushstring, "55 8B EC 8B 55 0C 85 D2 75 0D", false);
	scanner.QueueSigScan("lua_pushnil", (DWORD*)&rlua_pushnil, "55 8B EC 8B 4D 08 8B 41 0C C7 40", true);
	scanner.QueueSigScan("lua_pushnumber", (DWORD*)&rlua_pushnumber, "55 8B EC 8B 4D 08 0F 28 15", true);
	scanner.QueueSigScan("lua_pushvalue", (DWORD*)&rlua_pushvalue, "55 8B EC 56 FF 75 0C 8B 75 08 56 E8 ? ? ? ? 8B 56 0C 83 C4 08", true);
	scanner.QueueSigScan("lua_pushboolean", (DWORD*)&rlua_pushboolean, "55 8B EC 8B 55 08 33 C0 39 45 0C", true);
	scanner.QueueSigScan("lua_pushlightuserdata", (DWORD*)&rlua_pushlightuserdata, "55 8B EC 8B 55 08 8B 45 0C 8B 4A 0C", true);
	scanner.QueueSigScan("lua_tolstring", (DWORD*)&rlua_tolstring, "55 8B EC 56 FF 75 0C 8B 75 08 56 E8 ? ? ? ? 8B D0", true);
	scanner.QueueSigScan("lua_topointer", (DWORD*)&rlua_topointer, "55 8B EC FF 75 0C FF 75 08 E8 ? ? ? ? 83 C4 08 8B 48 08 49", false);
	scanner.QueueSigScan("lua_tonumber", (DWORD*)&rlua_tonumber, "55 8B EC 83 EC 10 FF 75 0C FF 75 08 E8", false);
	scanner.QueueSigScan("lua_toboolean", (DWORD*)&rlua_toboolean, "55 8B EC FF 75 0C FF 75 08 E8 ? ? ? ? 83 C4 08 8B 48 08 85 C9", false);
	scanner.QueueSigScan("lua_settop", (DWORD*)&rlua_settop, "55 8B EC 8B 55 0C 85 D2 78 38", true);
	scanner.QueueSigScan("lua_remove", (DWORD*)&rlua_remove, "55 8B EC 56 FF 75 0C 8B 75 08 56 E8 ? ? ? ? 83 C4 08 8D 48 10", true);
	scanner.QueueSigScan("lua_pcall", (DWORD*)&rlua_pcall, "55 8B EC 8B 45 14 83 EC 08 57", true);
	scanner.QueueSigScan("lua_type", (DWORD*)&rlua_type, "55 8B EC FF 75 0C FF 75 08 E8 ? ? ? ? 83 C4 08 3D ? ? ? ? 75 05", false);
	scanner.QueueSigScan("lua_xmove", (DWORD*)&rlua_xmove, "55 8B EC 56 8B 75 0C 57 8B 7D 08 3B FE", true);
	scanner.QueueSigScan("lua_next", (DWORD*)&rlua_next, "55 8B EC 56 FF 75 0C 8B 75 08 56 E8 ? ? ? ? 8B 4E 0C 83 E9 10 51 FF 30 56", true);
	scanner.QueueSigScan("luaL_ref", (DWORD*)&rluaL_ref, "55 8B EC 56 8B 75 08 57 8B 7D 0C 8D 87 ? ? ? ? 3D ? ? ? ? 77 0C 8B 46 0C 47 2B 46 1C C1 F8 04 03 F8 8B 46 0C", true);
	scanner.QueueSigScan("luaL_unref", (DWORD*)&rluaL_unref, "55 8B EC 53 8B 5D 10 85 DB 78 47", false);
	scanner.QueueSigScan("luaL_getmetafield", (DWORD*)&rluaL_getmetafield, "55 8B EC 56 FF 75 0C 8B 75 08 56 E8 ? ? ? ? 83 C4 08 85 C0 74 57", false);
	scanner.QueueSigScan("luaD_poscall", (DWORD*)&rluaD_poscall, "55 8B EC 51 53 56 57 8B 7D 08 F6 47 38 02", false);
	scanner.QueueSigScan("sandboxthread", (DWORD*)&sandboxthread, "55 8B EC 56 8B 75 08 6A 00 6A 00 56 E8 ? ? ? ? 6A 00", false);
	scanner.QueueSigScan("datamodel_func", (DWORD*)&DataModelFunc, "55 8B EC 6A FF 68 ? ? ? ? 64 A1 ? ? ? ? 50 64 89 25 ? ? ? ? 51 57 8B F9 C7 07", false);
	
	std::vector<std::string> results = scanner.Run();
	scanner.Clear();
	std::cout << "Failed sigscans: " << results.size() << std::endl;

	if (results.size() == 0) {
		DataModelVF = *(DWORD*)(DataModelFunc + DATAMODEL_VF_OFF);
		return 1;
	} else {
		for (std::string value : results) {
			std::cout << "-> " << value << std::endl;
		}
		return 0;
	}
}

void Main() {
	DWORD oldProtection;
	VirtualProtect(&FreeConsole, 1, PAGE_EXECUTE_READWRITE, &oldProtection);
	*(BYTE*)&FreeConsole = 0xC3;
	VirtualProtect(&FreeConsole, 1, oldProtection, NULL);

	AllocConsole();
	freopen("CONIN$", "r", stdin);
	freopen("__data.txt", "w", stdout);
	SetConsoleTitleA("RBX Wrapper Remade");

	Check(InitScan(), "scan sigs.");
	DataModel = Scanner::PatternScan((const char*)&DataModelVF, "xxxx");
	Check(DataModel, "obtain datamodel.");
	ScriptContext = RBXLib::FindFirstChildOfClass(DataModel, "ScriptContext");
	Check(ScriptContext, "obtain scriptcontext.");
	RLuaState = *(DWORD*)(ScriptContext + 164) - (ScriptContext + 164);
	Check(RLuaState, "obtain luastate.");

	CallCheck::Setup();

	std::cout << "-/\\-DEBUG INFO-/\\-" << std::endl;
	std::cout << "Base: " << (DWORD*)GetModuleHandle(NULL) << std::endl;
	std::cout << "Datamodel: " << (DWORD*)DataModel << std::endl;
	std::cout << "ScriptContext: " << (DWORD*)ScriptContext << std::endl;
	std::cout << "Luastate: " << (DWORD*)RLuaState << std::endl;
	std::cout << "-/\\------------/\\-" << std::endl;

	while (true) {
		std::cout << "> ";
		std::string input;
		std::getline(std::cin, input);

		lua_State *L = luaL_newstate();
		luaL_openlibs(L);

		DWORD RL = rlua_newthread(RLuaState);
		sandboxthread(RL);

		std::cout << "Main RL: " << (DWORD*)RL << std::endl;
		std::cout << "Main L:  " << L << std::endl;

		PairStates(RL, L);

		std::cout << "States have been paired!" << std::endl;

		InitLuaState(L);
		
		try {
			std::string code = input;
		    //std::string code = "spawn(function() script = Instance.new('LocalScript') " + input + " end)";
			if (luaL_loadstring(L, code.c_str())) {
				std::string Error = lua_tostring(L, -1);
				std::cout << "Compile-time error: " << Error << std::endl;

			} else {
				int Result = lua_resume(L, 0);
				if (Result) {
					if (Result == 1) {
						std::cout << "The thread yielded." << std::endl;
					} else {
						std::string Error = lua_tostring(L, -1);
						std::cout << "Run-time error: " << Error << std::endl;
					}
				} else {
					std::cout << "Ran successfully." << std::endl;
				}
			}
		} catch (std::exception Error) {
			std::cout << "C Error: " << Error.what() << std::endl;
		}
		
	}
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
	if (fdwReason == DLL_PROCESS_ATTACH) {
		DisableThreadLibraryCalls(hinstDLL);
		CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)Main, NULL, NULL, NULL);
	}
	return TRUE;
}