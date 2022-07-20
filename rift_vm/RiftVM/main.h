#pragma once

#include <Windows.h>
#include <iostream>
#include <string>
#include <libtcc.h>

#include "scanner.h"
#include "retcheck.h"
#include "r_lua.h"
#include "code.h"

#define base			(DWORD)GetModuleHandle(NULL)
#define sc_vftable		0xD8A75C
#define version			"1.0"

DWORD scriptcontext;
DWORD luastate;

DWORD GetLuaState();
void Execute(std::string code);
void Setup_Env(TCCState *s);
void rLua_Init();
void Main();
