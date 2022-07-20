#include "main.h"

DWORD GetLuaState() {
	return luastate;
}

void Execute(std::string code) {
	std::string syntax = lua_syntaxcheck(code);
	if (syntax == "0") {
		lua_State *state = luaL_newstate();
		luaL_openlibs(state);

		if (lua_execute(state, base_lua) != 0)
			return;
		if (lua_execute(state, table2_lua) != 0)
			return;
		if (lua_execute(state, string2_lua) != 0)
			return;
		if (lua_execute(state, lexer_lua) != 0)
			return;
		if (lua_execute(state, gg_lua) != 0)
			return;
		if (lua_execute(state, mlplexer_lua) != 0)
			return;
		if (lua_execute(state, mlpmisc_lua) != 0)
			return;
		if (lua_execute(state, mlptable_lua) != 0)
			return;
		if (lua_execute(state, mlpmeta_lua) != 0)
			return;
		if (lua_execute(state, mlpexpr_lua) != 0)
			return;
		if (lua_execute(state, mlpstat_lua) != 0)
			return;
		if (lua_execute(state, mlpext_lua) != 0)
			return;
		if (lua_execute(state, ast2cast_lua) != 0)
			return;
		if (lua_execute(state, cast2string_lua) != 0)
			return;

		lua_pushstring(state, code.c_str());
		lua_setglobal(state, "src");

		if (lua_execute(state, lua2c_lua) != 0)
			return;

		lua_getglobal(state, "code");
		const char *c_code = lua_tostring(state, -1);
		lua_close(state);

		std::string b_code = c_code;
		b_code = b_code + code_bridge;
		b_code = code_headers + b_code;
		c_code = b_code.c_str();

		std::cout << c_code << std::endl;

		TCCState *s = tcc_new();
		if (s) {
			tcc_set_output_type(s, TCC_OUTPUT_MEMORY);
			if (tcc_compile_string(s, c_code) != -1) {
				std::cout << "Running!" << std::endl;
				Setup_Env(s);
				tcc_run(s, NULL, NULL);
				tcc_delete(s);
			}
			else {
				std::cout << "ERROR: Failed to compile internal code" << std::endl;
			}
		}
		else {
			std::cout << "ERROR: Failed to create internal state" << std::endl;
		}
	}
	else {
		std::cout << syntax << std::endl;
	}
}

void Setup_Env(TCCState *s) {
	tcc_add_symbol(s, "GetLuaState", GetLuaState);
	tcc_add_symbol(s, "luaL_error", r_luaL_error);
	tcc_add_symbol(s, "lua_equal", r_lua_equal);
	tcc_add_symbol(s, "lua_lessthan", r_lua_lessthan);
	tcc_add_symbol(s, "lua_gettop", r_lua_gettop);
	tcc_add_symbol(s, "lua_insert", r_lua_insert);
	tcc_add_symbol(s, "lua_remove", r_lua_remove);
	tcc_add_symbol(s, "lua_pushnil", r_lua_pushnil);
	tcc_add_symbol(s, "lua_pushboolean", r_lua_pushboolean);
	tcc_add_symbol(s, "lua_pushvalue", r_lua_pushvalue);
	tcc_add_symbol(s, "lua_settop", r_lua_settop);
	tcc_add_symbol(s, "lua_type", r_lua_type);
	tcc_add_symbol(s, "lua_typename", r_lua_typename);
	tcc_add_symbol(s, "lua_iscfunction", r_lua_iscfunction);
	tcc_add_symbol(s, "lua_isnumber", r_lua_isnumber);
	tcc_add_symbol(s, "lua_isstring", r_lua_isstring);
	tcc_add_symbol(s, "lua_isuserdata", r_lua_isuserdata);
	tcc_add_symbol(s, "lua_toboolean", r_lua_toboolean);
	tcc_add_symbol(s, "lua_tolstring", r_lua_tolstring);
	tcc_add_symbol(s, "lua_concat", r_lua_concat);
	tcc_add_symbol(s, "lua_createtable", r_lua_createtable);
	tcc_add_symbol(s, "lua_getfield", r_lua_getfield);
	tcc_add_symbol(s, "lua_setfield", r_lua_setfield);
	tcc_add_symbol(s, "lua_pcall", r_lua_pcall);
	tcc_add_symbol(s, "lua_pushstring", r_lua_pushstring);
	tcc_add_symbol(s, "lua_pushnum", r_lua_pushnumber);
	tcc_add_symbol(s, "lua_newthread", r_lua_newthread);
	tcc_add_symbol(s, "lua_objlen", r_lua_objlen);
	tcc_add_symbol(s, "lua_pushcclosure", r_lua_pushcclosure);
	tcc_add_symbol(s, "lua_rawseti", r_lua_rawseti);
	tcc_add_symbol(s, "lua_rawgeti", r_lua_rawgeti);
	tcc_add_symbol(s, "lua_tonumber", r_lua_tonumber);
	tcc_add_symbol(s, "lua_replace", r_lua_replace);
}

// here, we grab the functions from the roblox lua api
void rLua_Init() {
	r_lua_concat = (PROTO_concat)unprotect(r_concat + base);
	r_lua_createtable = (PROTO_createtable)unprotect(r_createtable + base);
	r_lua_getfield = (PROTO_getfield)unprotect(r_getfield + base);
	r_lua_setfield = (PROTO_setfield)unprotect(r_setfield + base);
	r_lua_pcall = (PROTO_pcall)unprotect(r_pcall + base);
	r_lua_pushstring = (PROTO_pushstring)unprotect(r_pushstring + base);
	r_lua_pushnumber = (PROTO_pushnumber)unprotect(r_pushnumber + base);
	r_lua_newthread = (PROTO_newthread)unprotect(r_newthread + base);
	r_lua_objlen = (PROTO_objlen)unprotect(r_objlen + base);
	r_lua_pushcclosure = (PROTO_pushcclosure)unprotect(r_pushcclosure + base);
	r_lua_rawseti = (PROTO_rawseti)unprotect(r_rawseti + base);
	r_lua_rawgeti = (PROTO_rawgeti)unprotect(r_rawgeti + base);
	r_lua_tonumber = (PROTO_tonumber)unprotect(r_tonumber + base);
	r_lua_replace = (PROTO_replace)unprotect(r_replace + base);
	setthreadidentity = (PROTO_setthreadidentity)(r_setthreadidentity + base);
}

void Main() {
	DWORD oldProtection;
	VirtualProtect((LPVOID)&FreeConsole, 1, PAGE_EXECUTE_READWRITE, &oldProtection);
	*(BYTE*)&FreeConsole = 0xC3;
	VirtualProtect((LPVOID)&FreeConsole, 1, PAGE_EXECUTE_READ, &oldProtection);

	AllocConsole();
	SetConsoleTitleA("Rift VM - Console");
	freopen("CONIN$", "r", stdin);
	freopen("CONOUT$", "w", stdout);

	std::cout << "/\\Rift Virtual Machine/\\" << std::endl;
	std::cout << "Version: " << version << std::endl;

	DWORD sc_ptr = sc_vftable + base;
	scriptcontext = Scan((PBYTE)&sc_ptr, "xxxx");
	luastate = *(DWORD*)(scriptcontext + 164) - (scriptcontext + 164);

	std::cout << "ScriptContext: " << scriptcontext << std::endl;
	std::cout << "LuaState: " << luastate << std::endl;

	luastate = r_lua_newthread(luastate);
	std::cout << "New Thread - LuaState: " << luastate << std::endl;

	uint64_t obj = 0;
	setthreadidentity(luastate, 7, (int*)&obj);
	std::cout << "Identity: " << (luastate ? *(DWORD *)(luastate - 20) << 27 >> 27 : 0) << std::endl;

	std::cout << "/\\Loaded/\\" << std::endl;

	r_lua_getglobal(luastate, "print");
	r_lua_pushstring(luastate, "/\\ Rift Virtual Machine Loaded /\\");
	r_lua_pcall(luastate, 1, 0, 0);


	//Execute("print('FilteringEnabled: ' .. tostring(game.Workspace.FilteringEnabled))");

	while (true) {
		std::string input;
		std::cout << "> ";
		getline(std::cin, input);
		Execute(input);
	}
}

BOOL APIENTRY DllMain(HMODULE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
	if (fdwReason == DLL_PROCESS_ATTACH) {
		DisableThreadLibraryCalls(hinstDLL);
		CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)rLua_Init, NULL, NULL, NULL);
		CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)Main, NULL, NULL, NULL);
	}
	return TRUE;
}