#include <Windows.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
extern "C" {
	#include "lua.h"
	#include "lauxlib.h"
	#include "lualib.h"
	#include "lobject.h"
	#include "lopcodes.h"
	#include "lstate.h"
	#include "llimits.h"
	#include "lfunc.h"
	#include "lmem.h"
	#include "lstring.h"
	#include "lundump.h"
}

#define ISCONSTANT(x,p)		(x <= p->sizelocvars - 1)	

#define NEWLOCAL(l,pc)		(l->startpc == pc)
#define LOCALINSCOPE(l,pc)	(l->startpc <= pc && l->endpc >= pc)
#define CHECK(x,e)			if (!(x)) {\
								std::cout << "Error: " << e << std::endl; \
								std::cout << "ABORTING" << std::endl; \
								return; \
							}

typedef unsigned int uint;

int ProtoWriter(lua_State *L, const void *p, size_t size, void *ud) {
	std::vector<UCHAR>* Dump = (std::vector<UCHAR>*)ud;
	for (size_t i = 0; i < size; i++)
		Dump->push_back(((BYTE*)p)[i]);
	return 0;
}

void DumpProto(lua_State *L, Proto *p, std::string FileName) {
	std::vector<UCHAR> Result;
	luaU_dump(L, p, ProtoWriter, &Result, 0);
	std::ofstream file(FileName, std::ofstream::out | std::ofstream::binary);
	if (!file.is_open())
		std::cout << "Could not open file: " << FileName << std::endl;
	else 
		for (size_t i = 0; i < Result.size(); i++)
			file.put(Result[i]);
	file.close();
}

std::string ReadTValue(TValue *k, bool raw = false) {
	switch (k->tt) {
		case LUA_TNUMBER: {
			/* https://stackoverflow.com/questions/13686482/c11-stdto-stringdouble-no-trailing-zeros */
			std::string Number{ std::to_string(k->value.n) };
			int offset{ 1 }; 
			if (Number.find_last_not_of('0') == Number.find('.'))
				{ offset = 0; }
			Number.erase(Number.find_last_not_of('0') + offset, std::string::npos);
			return Number;
		}
		case LUA_TSTRING: {
			if (raw)
				return std::string(svalue(k));
			else
				return "\"" + std::string(svalue(k)) + "\"";
		}
	}
}

void TestProto(Proto *p) {
	for (int i = 0; i < p->sizelocvars; i++) {
		std::cout << getstr(p->locvars[i].varname) << std::endl;
		std::cout << p->locvars[i].startpc << std::endl;
		std::cout << p->locvars[i].endpc << std::endl;
	}
}

void ReadProto(Proto *p) {
	std::string Source = "";

	/*
	std::cout << "---------" << std::endl;
	std::cout << "Name: " << P->source << std::endl;
	std::cout << "First line: " << P->linedefined << std::endl;
	std::cout << "Last line: " << P->lastlinedefined << std::endl;
	std::cout << "Size upvalue (names): " << P->sizeupvalues << std::endl;
	std::cout << "Number of upvalues: " << (int)P->nups << std::endl;
	std::cout << "Number of params: " << (int)P->numparams << std::endl;
	std::cout << "Is vararg: " << (int)P->is_vararg << std::endl;
	std::cout << "Number of protos: " << P->sizep << std::endl;
	*/

	/*
	todo:

		when doing local variable assignment, check if the local variable EXISTS in the scope
		(check if local var pc is in startpc - endpc)

		if it isn't GLOBAL, not local
	
	*/

	/*
		multiple assignments
		ex: local a,b,c = 4,5,6
		will consist of several loadks, but the startpc will be of the last loadk
	
	*/

	/* do end block (note to self)
	
		do-end blocks are only useful for scoping LOCAL variables, so all of them will do some sort of loading.
		check to see if it is a local && is a new one

		then see if the end pc matches that of the endpc of the proto structure
		if not, this variable ends somewhere else, meaning it starts at the beginning of the do end.

		in examples like this:

		do
			local test = 4
			for i,v in next, game.Players:GetPlayers() do end
		end

		all of that will be in the do end, but in this:

		do
		
			for i,v in next, game.Players:GetPlayers() do end
			local test = 4
		end

		only the bottom portion will, which makes sense because do-end blocks are only useful for scoping.
	
	*/

	std::map<int, TValue*>TempRegisters;

	/* check for local variable initialization at pc = 0 */
	for (int i = 0; i < p->sizelocvars; i++)
		if (p->locvars[i].startpc == 0)
			Source = Source + "local " + getstr(p->locvars[i].varname) + "\n";
	
	for (int i = 0, pc = 1; i < p->sizecode; i++, pc++) {

		/* read instruction */
		Instruction inst = p->code[i];
		OpCode op = GET_OPCODE(inst);
		uint RA = GETARG_A(inst);
		uint RB = GETARG_B(inst);
		uint RC = GETARG_C(inst);
		uint RBx = GETARG_Bx(inst);
		uint RsBx = GETARG_sBx(inst);

		switch (op) {
			case OP_MOVE: {
				/* to do: CLOSUREs */

				/* is op_move even used in cases that aren't about local variables? */
				/* local := */
				if (ISCONSTANT(RA, p)) {
					/* has it been defined yet? */
					LocVar *LocalA = &p->locvars[RA];
					/* to do: work on LOCALINSCOPE, or is that not necessary? */
					if (NEWLOCAL(LocalA, pc))
						Source = Source + "local ";
					Source = Source + getstr(LocalA->varname) + " = ";
					/* RB should be another constant or some temp register, otherwise SETGLOBAL will be used, not OP_MOVE */
					CHECK(ISCONSTANT(RB, p) || TempRegisters.find(RB) != TempRegisters.end(), "Origin of OP_MOVE value unknown");
					if (ISCONSTANT(RB, p)) {
						LocVar *LocalB = &p->locvars[RB];
						Source = Source + getstr(LocalB->varname) + "\n";
					} else {
						Source = Source + ReadTValue(TempRegisters[RB]) + "\n";
					}
				} else {
					CHECK(TempRegisters.find(RA) == TempRegisters.end(), "Nothing in temp register of RA");
					/* has it been defined yet? */
					LocVar *LocalA = &p->locvars[RA];
					/* to do: work on LOCALINSCOPE, or is that not necessary? */
					if (NEWLOCAL(LocalA, pc))
						Source = Source + "local ";
					Source = Source + getstr(LocalA->varname) + " = ";
					
				}
				break;
			}
			case OP_LOADK: {
				TValue *Constant = &p->k[RBx];

				/* is it a local? register points to local pool */
				if (ISCONSTANT(RA, p)) {
					/* has it been defined yet? */
					LocVar *Local = &p->locvars[RA];
					if (LOCALINSCOPE(Local, pc)) {
						if (NEWLOCAL(Local, pc))
							Source = Source + "local ";
						Source = Source + getstr(Local->varname) + " = " + ReadTValue(Constant) + "\n";
						break;
					}
				}

				/* more opcodes (searching for SETGLOBAL) */
				if (pc <= p->sizecode) {
					Instruction next = p->code[pc];
					/* next opcode should be SETGLOBAL */
					if (GET_OPCODE(next) == OP_SETGLOBAL) {
						TValue *GlobalName = &p->k[GETARG_Bx(next)];
						Source = Source + ReadTValue(GlobalName, true) + " = " + ReadTValue(Constant) + "\n";
						break;
					}
				}

				/* load value into temp register */
				TempRegisters[RA] = Constant;

				/*
				CHECK(pc <= p->sizecode, "Expected another opcode after LOADK");
				Instruction next = p->code[pc];
				CHECK(GET_OPCODE(next) == OP_SETGLOBAL, "Expected SETGLOBAL after LOADK");
				TValue *GlobalName = &p->k[GETARG_Bx(next)];
				Source = Source + ReadTValue(GlobalName, true) + " = " + ReadTValue(Constant) + "\n";	*/
				break;
			}
			case OP_LOADBOOL: {
				/* regular constant loading */
				if (RC == 0) {
					// TValue *Constant = &p
				} else {

				}
			}
			case OP_LOADNIL: {
				/* load nil works for multiple registers ugh */
				/* is it a local? register points to local pool */
				if (ISCONSTANT(RA, p)) {
					/* has it been defined yet? */
					LocVar *Local = &p->locvars[RA];
					if (LOCALINSCOPE(Local, pc)) {
						if (NEWLOCAL(Local, pc))
							Source = Source + "local " + getstr(Local->varname) + "\n";
						else
							Source = Source + getstr(Local->varname) + " = nil\n";
						break;
					}
				} 
				/* next opcode should be SETGLOBAL */
				CHECK(pc <= p->sizecode, "Expected another opcode after LOADK");
				Instruction next = p->code[pc];
				CHECK(GET_OPCODE(next) == OP_SETGLOBAL, "Expected SETGLOBAL after LOADK");
				TValue *GlobalName = &p->k[GETARG_Bx(next)];
				Source = Source + ReadTValue(GlobalName, true) + " = nil\n";
				break;
			}
			case OP_GETUPVAL: {

			}
			case OP_GETGLOBAL: {

			}
		}

	}

	std::cout << "==== DECOMPILED ====" << std::endl;
	std::cout << Source << std::endl;
}

int main() {
	lua_State *L = luaL_newstate();
	luaL_openlibs(L);

	while (true) {
		std::cout << "--> unlua <---" << std::endl;
		std::cout << "[1]: dump bytecode to file" << std::endl;
		std::cout << "[2]: decompile bytecode" << std::endl;
		std::cout << "[3]: interactive test" << std::endl;
		std::cout << "[4]: exit (safely)" << std::endl;

		std::string Response;
		do {
			std::cout << "Option > ";
			getline(std::cin, Response);
		} while (Response != "1" && Response != "2" && Response != "3" && Response != "4");

		if (Response == "1") {
			std::string InputFile;
			std::cout << "Input File > ";
			getline(std::cin, InputFile);
		
			int Error = luaL_loadfile(L, InputFile.c_str());
			if (Error == 0) {
				Proto *p = clvalue(L->top - 1)->l.p;

				std::cout << "Loaded file" << std::endl;
				std::string OutputFile;
				std::cout << "Output File > ";
				getline(std::cin, OutputFile);

				DumpProto(L, p, OutputFile);

				std::cout << "Dump successful" << std::endl;
			} else {
				std::cout << "Error code: " << Error << std::endl;
			}
		} else if (Response == "2") {
			std::string InputFile;
			std::cout << "Input File > ";
			getline(std::cin, InputFile);
			int Error = luaL_loadfile(L, InputFile.c_str());
			if (Error == 0) {
				std::cout << "Loaded file" << std::endl;

				Proto *p = clvalue(L->top - 1)->l.p;

				ReadProto(p);
			} else {
				std::cout << "Error code: " << Error << std::endl;
			}
		} else if (Response == "3") {
			while (true) {
				std::string Code;
				std::cout << ">> ";
				getline(std::cin, Code);
				if (Code == "exit")
					break;
				int Error = luaL_loadstring(L, Code.c_str());
				Proto *p = clvalue(L->top - 1)->l.p;

				TestProto(p);
			}
		} else if (Response == "4") {
			lua_close(L);
			return 0;
		}
	}
}