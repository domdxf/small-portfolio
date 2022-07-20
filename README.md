This repository contains some pretty old projects (~2017) of mine related to game development (in Lua), reverse engineering/game security (mostly related to Roblox), and general programming. They're old projects b/c that's when I first started to get into RE before stopping for a while to pursue game development.

## Overview (oldest -> newest)
- rift_vm (C++): script execution exploit for the Roblox engine
- lua_wrapper (C++): same as above (different method), newer
- \***riftv2 (C++)**: same as above (again, different method), **newest**
- game_dev (Lua): game development

Ignore the formalities lol, this was written for applying to places.

## rift_vm
- Roblox is a popular game engine that allows players to develop games that are written in Lua (technically, a Roblox-modified version of Lua). Generally, only authorized game scripts scripted by the developer can be run. Any method that achieves (unauthorized) script execution by the client is considered an exploit. Achieving script execution is a bit harder in Roblox because the Lua (and consequently bytecode) used is different from vanilla Lua, requiring some reversing to figure out. This project implements a way to achieve script execution in a novel way that overcomes this, by converting the desired script to execute (written in Lua) to Lua C calls (which are mapped to the Lua C calls in the Roblox engine, and called via DLL injection) that are then executed by an embedded C compiler (tcc).
- This project also implements a way to circumvent a security check in the Roblox engine ("retcheck"): certain functions check the return address in order to detect unauthorized calls; this copies these functions (adjusting function calls as necessary) into a portion of memory and removes the checks.

## lua_wrapper
- (see rift_vm for context): This projects implements yet another way to achieve script execution without the need for converting between vanilla Lua to roblox Lua bytecode. It does so by creating a one-to-one correspondence between a vanilla Lua VM and a roblox Lua VM. That is, to achieve script execution, the desired script is run in a vanilla Lua VM whose internal calls are mapped to Roblox's Lua C API calls (hence the name "wrapper").
- This project implements a bypass for "retcheck" (see above), this time in shorter lines of codes (taking advantage of the similarity of retcheck's x86 asm code) because it removes the need for a disassembler (hde32).
- This project also implements a bypass for "call check," a security check in the Roblox Lua engine that checks to see if C functions are called that are outside of an allowed address range. It does so by passing an address in the address range that will lead to an exception, where the call can then be rerouted to the desired C function to call.

## riftv2
- A cleaner, more modern implementation of a script execution method.
- This is a full fledged project, with a UI (omitted), injector (omitted), updater (omitted). What is shown is the underlying script execution method, which is more relevant.
- This project implements a third way to achieve script execution. This method injects Roblox-Lua-precompiled bytecode of a lua VM written in lua, a clean approach at allowing script execution.
- Like the past projects, it implements a way to bypass retcheck and call check, though both methods are now cleaner. The call check bypass makes a hook in GetModuleHandleA.

## game_dev
- Just one of the files in my game -- can't really include more b/c it's a closed source game w/ active players. This file implements saving for "builds."

## unlua (old, but decided to put at bottom since it is incomplete)
- An abandoned project that aimed to make a better Lua bytecode -> Lua source decompiler.