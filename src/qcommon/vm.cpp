/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2000-2013 Darklegion Development

This file is part of Tremulous.

Tremulous is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Tremulous is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Tremulous; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// vm.c -- virtual machine

//
// intermix code and data symbol table
// 
// a dll has one imported function: VM_SystemCall
// and one exported function: Perform
//

#include "vm.h"
#include "../sys/sys_shared.h"
#include "cmd.h"
#include "cvar.h"
#include "files.h"
#include "vm_local.h"

#include <type_traits>

//static_assert(std::is_pod<vm_s>::value,
//        "Error: struct vm_s is not POD");

vm_t *currentVM = NULL;
vm_t *lastVM = NULL;
int vm_debugLevel;

// used by Com_Error to get rid of running vm's before longjmp
static int forced_unload;

#define MAX_VM 3
//vm_t vmTable[MAX_VM];

void VM_VmProfile_f(void);

void VM_Debug(int level) { vm_debugLevel = level; }

void VM_Init(void)
{
    Cvar_Get("vm_cgame", "2", CVAR_ARCHIVE);  // !@# SHIP WITH SET TO 2
    Cvar_Get("vm_game", "2", CVAR_ARCHIVE);  // !@# SHIP WITH SET TO 2
    Cvar_Get("vm_ui", "2", CVAR_ARCHIVE);  // !@# SHIP WITH SET TO 2

    Cmd_AddCommand("vmprofile", VM_VmProfile_f);
}

static int ParseHex(const char *text)
{
    int value;
    int c;

    value = 0;
    while ((c = *text++) != 0)
    {
        if (c >= '0' && c <= '9')
        {
            value = value * 16 + c - '0';
            continue;
        }
        if (c >= 'a' && c <= 'f')
        {
            value = value * 16 + 10 + c - 'a';
            continue;
        }
        if (c >= 'A' && c <= 'F')
        {
            value = value * 16 + 10 + c - 'A';
            continue;
        }
    }

    return value;
}

/*
============
VM_DllSyscall

Dlls will call this directly

 rcg010206 The horror; the horror.

  The syscall mechanism relies on stack manipulation to get its args.
   This is likely due to C's inability to pass "..." parameters to
   a function in one clean chunk. On PowerPC Linux, these parameters
   are not necessarily passed on the stack, so while (&arg[0] == arg)
   is true, (&arg[1] == 2nd function parameter) is not necessarily
   accurate, as arg's value might have been stored to the stack or
   other piece of scratch memory to give it a valid address, but the
   next parameter might still be sitting in a register.

  Quake's syscall system also assumes that the stack grows downward,
   and that any needed types can be squeezed, safely, into a signed int.

  This hack below copies all needed values for an argument to a
   array in memory, so that Quake can get the correct values. This can
   also be used on systems where the stack grows upwards, as the
   presumably standard and safe stdargs.h macros are used.

  As for having enough space in a signed int for your datatypes, well,
   it might be better to wait for DOOM 3 before you start porting.  :)

  The original code, while probably still inherently dangerous, seems
   to work well enough for the platforms it already works on. Rather
   than add the performance hit for those platforms, the original code
   is still in use there.

  For speed, we just grab 15 arguments, and don't worry about exactly
   how many the syscall actually needs; the extra is thrown away.

============
*/
intptr_t QDECL VM_DllSyscall(intptr_t arg, ...)
{
#if !id386 || defined __clang__
    // rcg010206 - see commentary above
    intptr_t args[MAX_VMSYSCALL_ARGS];
    int i;
    va_list ap;

    args[0] = arg;

    va_start(ap, arg);
    for (i = 1; i < ARRAY_LEN(args); i++) args[i] = va_arg(ap, intptr_t);
    va_end(ap);

    return currentVM->systemCall(args);
#else  // original id code
    return currentVM->systemCall(&arg);
#endif
}

/*
=================
VM_LoadQVM

Load a .qvm file
=================
*/
vmHeader_t *VM_LoadQVM(vm_t *vm, bool alloc, bool unpure)
{
    int dataLength;
    int i;
    char filename[MAX_QPATH];
    union {
        vmHeader_t *h;
        void *v;
    } header;

    // load the image
    Com_sprintf(filename, sizeof(filename), "vm/%s.qvm", vm->name);
    Com_Printf("Loading vm file %s...\n", filename);

    FS_ReadFileDir(filename, vm->searchPath, unpure, &header.v);

    if (!header.h)
    {
        Com_Printf("Failed.\n");
        delete vm;

        Com_Printf(S_COLOR_YELLOW "Warning: Couldn't open VM file %s\n", filename);

        return NULL;
    }

    // show where the qvm was loaded from
    FS_Which(filename, vm->searchPath);

    if (LittleLong(header.h->vmMagic) == VM_MAGIC_VER2)
    {
        Com_Printf("...which has vmMagic VM_MAGIC_VER2\n");

        // byte swap the header
        for (i = 0; i < sizeof(vmHeader_t) / 4; i++)
        {
            ((int *)header.h)[i] = LittleLong(((int *)header.h)[i]);
        }

        // validate
        if (header.h->jtrgLength < 0 || header.h->bssLength < 0 || header.h->dataLength < 0 ||
            header.h->litLength < 0 || header.h->codeLength <= 0)
        {
            delete vm;
            FS_FreeFile(header.v);

            Com_Printf(S_COLOR_YELLOW "Warning: %s has bad header\n", filename);
            return NULL;
        }
    }
    else if (LittleLong(header.h->vmMagic) == VM_MAGIC)
    {
        // byte swap the header
        // sizeof( vmHeader_t ) - sizeof( int ) is the 1.32b vm header size
        for (i = 0; i < (sizeof(vmHeader_t) - sizeof(int)) / 4; i++)
        {
            ((int *)header.h)[i] = LittleLong(((int *)header.h)[i]);
        }

        // validate
        if (header.h->bssLength < 0 || header.h->dataLength < 0 || header.h->litLength < 0 || header.h->codeLength <= 0)
        {
            delete vm;
            FS_FreeFile(header.v);

            Com_Printf(S_COLOR_YELLOW "Warning: %s has bad header\n", filename);
            return NULL;
        }
    }
    else
    {
        delete vm;
        FS_FreeFile(header.v);

        Com_Printf(S_COLOR_YELLOW "Warning: %s does not have a recognisable magic number in its header\n", filename);
        return NULL;
    }

    // round up to next power of 2 so all data operations can
    // be mask protected
    dataLength = header.h->dataLength + header.h->litLength + header.h->bssLength;
    for (i = 0; dataLength > (1 << i); i++)
    {
    }
    dataLength = 1 << i;

    if (alloc)
    {
        // allocate zero filled space for initialized and uninitialized data
        vm->dataBase = (unsigned char *)Hunk_Alloc(dataLength, h_high);
        vm->dataMask = dataLength - 1;
    }
    else
    {
        // clear the data, but make sure we're not clearing more than allocated
        if (vm->dataMask + 1 != dataLength)
        {
            delete vm;
            FS_FreeFile(header.v);

            Com_Printf(S_COLOR_YELLOW "Warning: Data region size of %s not matching after VM_Restart()\n", filename);
            return NULL;
        }

        ::memset(vm->dataBase, 0, dataLength);
    }

    // copy the intialized data
    ::memcpy(vm->dataBase, (byte *)header.h + header.h->dataOffset, header.h->dataLength + header.h->litLength);

    // byte swap the longs
    for (i = 0; i < header.h->dataLength; i += 4)
    {
        *(int *)(vm->dataBase + i) = LittleLong(*(int *)(vm->dataBase + i));
    }

    if (header.h->vmMagic == VM_MAGIC_VER2)
    {
        int previousNumJumpTableTargets = vm->numJumpTableTargets;

        header.h->jtrgLength &= ~0x03;

        vm->numJumpTableTargets = header.h->jtrgLength >> 2;
        Com_Printf("Loading %d jump table targets\n", vm->numJumpTableTargets);

        if (alloc)
        {
            vm->jumpTableTargets = (unsigned char *)Hunk_Alloc(header.h->jtrgLength, h_high);
        }
        else
        {
            if (vm->numJumpTableTargets != previousNumJumpTableTargets)
            {
                delete vm; 
                FS_FreeFile(header.v);

                Com_Printf(S_COLOR_YELLOW
                    "Warning: Jump table size of %s not matching after "
                    "VM_Restart()\n",
                    filename);
                return NULL;
            }

            ::memset(vm->jumpTableTargets, 0, header.h->jtrgLength);
        }

        ::memcpy(vm->jumpTableTargets,
            (byte *)header.h + header.h->dataOffset + header.h->dataLength + header.h->litLength, header.h->jtrgLength);

        // byte swap the longs
        for (i = 0; i < header.h->jtrgLength; i += 4)
        {
            *(int *)(vm->jumpTableTargets + i) = LittleLong(*(int *)(vm->jumpTableTargets + i));
        }
    }

    return header.h;
}

/*
=================
VM_Restart

Reload the data, but leave everything else in place
This allows a server to do a map_restart without changing memory allocation

We need to make sure that servers can access unpure QVMs (not contained in any pak)
even if the client is pure, so take "unpure" as argument.
=================
*/
vm_t *VM_Restart(vm_t *vm, bool unpure)
{
    vmHeader_t *header;

    // DLL's can't be restarted in place
    if (vm->dllHandle)
    {
        char name[MAX_QPATH];
        intptr_t (*systemCall)(intptr_t * parms);

        systemCall = vm->systemCall;
        Q_strncpyz(name, vm->name, sizeof(name));

        delete vm;

        vm = new vm_s(name, systemCall, VMI_NATIVE);
        return vm;
    }

    // load the image
    Com_Printf("VM_Restart()\n");

    if (!(header = VM_LoadQVM(vm, false, unpure)))
    {
        Com_Error(ERR_DROP, "VM_Restart failed");
        return NULL;
    }

    // free the original file
    FS_FreeFile(header);

    return vm;
}

void VM_Clear(void)
{
    int i;
    //for (i = 0; i < MAX_VM; i++) VM_Free(&vmTable[i]);
}

void VM_Forced_Unload_Start(void) { forced_unload = 1; }
void VM_Forced_Unload_Done(void) { forced_unload = 0; }

void *VM_ArgPtr(intptr_t intValue)
{
    if (!intValue)
    {
        return NULL;
    }
    // currentVM is missing on reconnect
    if (currentVM == NULL) return NULL;

    if (currentVM->entryPoint)
    {
        return (void *)(currentVM->dataBase + intValue);
    }
    else
    {
        return (void *)(currentVM->dataBase + (intValue & currentVM->dataMask));
    }
}

//=================================================================

static int QDECL VM_ProfileSort(const void *a, const void *b)
{
    vmSymbol_t *sa, *sb;

    sa = *(vmSymbol_t **)a;
    sb = *(vmSymbol_t **)b;

    if (sa->profileCount < sb->profileCount)
    {
        return -1;
    }
    if (sa->profileCount > sb->profileCount)
    {
        return 1;
    }
    return 0;
}

/*
==============
VM_VmProfile_f

==============
*/
void VM_VmProfile_f(void)
{
    vm_t *vm;
    vmSymbol_t **sorted, *sym;
    int i;
    double total;

    if (!lastVM)
    {
        return;
    }

    vm = lastVM;

    if (!vm->numSymbols)
    {
        return;
    }

    sorted = (vmSymbol_t **)Z_Malloc(vm->numSymbols * sizeof(*sorted));
    sorted[0] = vm->symbols;
    total = sorted[0]->profileCount;
    for (i = 1; i < vm->numSymbols; i++)
    {
        sorted[i] = sorted[i - 1]->next;
        total += sorted[i]->profileCount;
    }

    qsort(sorted, vm->numSymbols, sizeof(*sorted), VM_ProfileSort);

    for (i = 0; i < vm->numSymbols; i++)
    {
        int perc;

        sym = sorted[i];

        perc = 100 * (float)sym->profileCount / total;
        Com_Printf("%2i%% %9i %s\n", perc, sym->profileCount, sym->symName);
        sym->profileCount = 0;
    }

    Com_Printf("    %9.0f total\n", total);

    Z_Free(sorted);
}

/*
===============
VM_LogSyscalls

Insert calls to this while debugging the vm compiler
===============
*/
void VM_LogSyscalls(int *args)
{
    static int callnum;
    static FILE *f;

    if (!f)
    {
        f = fopen("syscalls.log", "w");
    }
    callnum++;
    fprintf(f, "%i: %p (%i) = %i %i %i %i\n", callnum, (void *)(args - (int *)currentVM->dataBase), args[0], args[1],
        args[2], args[3], args[4]);
}

// VM_BlockCopy
// Executes a block copy operation within currentVM data space
void VM_BlockCopy(unsigned int dest, unsigned int src, size_t n)
{
    unsigned int dataMask = currentVM->dataMask;

    if ((dest & dataMask) != dest || (src & dataMask) != src || ((dest + n) & dataMask) != dest + n ||
        ((src + n) & dataMask) != src + n)
    {
        Com_Error(ERR_DROP, "OP_BLOCK_COPY out of range!");
    }

    ::memcpy(currentVM->dataBase + dest, currentVM->dataBase + src, n);
}

/*
===============
vm_s::operator new

Allocate the VM in hunk memory.
===============
*/
void * vm_s::operator new(size_t sz)
{
    return Hunk_Alloc(sz, h_high);
}

/*
===============
vm_s::operator delete 

===============
*/
void vm_s::operator delete(void *ptr)
{
    Z_Free(ptr);
}

/*
===============
vm_s::vm_s

Constructor
===============
*/
vm_s::vm_s(const char *module, intptr_t (*systemCalls)(intptr_t *), vmInterpret_t interpret)
{
    vmHeader_t *header;
    int i, remaining, retval;
    char filename[MAX_OSPATH];
    void *startSearch = NULL;

    assert(module);
    assert(modlue[0]);
    assert(systemCalls);

    Q_strncpyz(name, module, sizeof(name));

    do {
        // FIXME: Make dllHandle an constructor argument; Otherwise an exception is required (which breaks RAII)
        retval = FS_FindVM(&startSearch, filename, sizeof(filename),
                module, interpret == VMI_NATIVE);

        if (retval == VMI_NATIVE)
        {
            // FIXME: Make dllHandle an constructor argument; Otherwise an exception is required (which breaks RAII)
            dllHandle = Sys_LoadGameDll(filename, &entryPoint, VM_DllSyscall);

            if (dllHandle)
                systemCall = systemCalls;
        }
        else if (retval == VMI_COMPILED)
        {
            searchPath = startSearch;
            if ((header = VM_LoadQVM(this, true, false)))
                break;

            // VM_Free overwrites the name on failed load
            Q_strncpyz(name, module, sizeof(name));
        }
    } while (retval >= 0);

    // FIXME: Make this impossible via the above FIXME's 
    //if (retval < 0)
    //    return nullptr;

    systemCall = systemCalls;

    // allocate space for the jump targets, which will be filled in by the compile/prep functions
    instructionCount = header->instructionCount;
    instructionPointers = (intptr_t *)Hunk_Alloc(instructionCount * sizeof(*instructionPointers), h_high);

    // copy or compile the instructions
    codeLength = header->codeLength;

    compiled = false;

#ifdef NO_VM_COMPILED
    if (interpret >= VMI_COMPILED)
    {
        Com_Printf("Architecture doesn't have a bytecode compiler, using interpreter\n");
        interpret = VMI_BYTECODE;
    }
#else
    if (interpret != VMI_BYTECODE)
    {
        compiled = true;
        VM_Compile(this, header);
    }
#endif
    // VM_Compile may have reset this->compiled if compilation failed
    if (!compiled)
        VM_PrepareInterpreter(this, header);

    // free the original file
    FS_FreeFile(header);

    // load the map file
    LoadSymbols();

    // the stack is implicitly at the end of the image
    programStack = dataMask + 1;
    stackBottom = programStack - PROGRAM_STACK_SIZE;
}

vm_s::~vm_s()
{
    if (destroy)
        destroy(this);

    if (dllHandle)
        Sys_UnloadDll(dllHandle);

	if ( codeBase )
		Z_Free( codeBase );

	if ( dataBase )
		Z_Free( dataBase );

	if ( instructionPointers )
		Z_Free( instructionPointers );
}

/*
===============
vm_s::ValueToSymbol

Assumes a program counter value
===============
*/
const char *vm_s::ValueToSymbol(int value)
{
    vmSymbol_t *sym = symbols;
    if (!sym) return "NO SYMBOLS";

    // find the symbol
    while (sym->next && sym->next->symValue <= value) sym = sym->next;

    if (value == sym->symValue) return sym->symName;

    // FIXME: Return a std::string
    static char text[MAX_TOKEN_CHARS];
    Com_sprintf(text, sizeof(text), "%s+%i", sym->symName, value - sym->symValue);

    return text;
}

/*
===============
vm_s::ValueToFunctionSymbol

For profiling, find the symbol behind this value
===============
*/
vmSymbol_t *vm_s::ValueToFunctionSymbol(int value)
{
    static vmSymbol_t nullSym;

    vmSymbol_t *sym = symbols;
    if (!sym) return &nullSym;

    while (sym->next && sym->next->symValue <= value) sym = sym->next;

    return sym;
}

/*
===============
vm_s::SymbolToValue
===============
*/
int vm_s::SymbolToValue(const char *symbol)
{
    for (vmSymbol_t *sym = symbols; sym; sym = sym->next)
    {
        if (!strcmp(symbol, sym->symName)) return sym->symValue;
    }

    return 0;
}


void *vm_s::ArgPtr(intptr_t intValue)
{
    if (!intValue) return nullptr;

    // currentVM is missing on reconnect here as well?
    if (!currentVM) return nullptr;

    if (entryPoint) return (void *)(dataBase + intValue);

    return (void *)(dataBase + (intValue & dataMask));
}

/*
==============
VM_Call


Upon a system call, the stack will look like:

sp+32	parm1
sp+28	parm0
sp+24	return value
sp+20	return address
sp+16	local1
sp+14	local0
sp+12	arg1
sp+8	arg0
sp+4	return stack
sp		return address

An interpreted function will immediately execute
an OP_ENTER instruction, which will subtract space for
locals from sp
==============
*/
intptr_t vm_s::Call(int callnum, ...)
{
    vm_t *oldVM;
    intptr_t r;

    oldVM = currentVM;
    currentVM = this;
    lastVM = this;

    ++callLevel;

    // if we have a dll loaded, call it directly
    if (entryPoint)
    {
        // rcg010207 -  see dissertation at top of VM_DllSyscall() in this file.
        int args[MAX_VMMAIN_ARGS - 1];
        va_list ap;

        va_start(ap, callnum);
        for (unsigned i = 0; i < ARRAY_LEN(args); i++) args[i] = va_arg(ap, int);
        va_end(ap);

        r = entryPoint(callnum, args[0], args[1], args[2]);
    }
    else
    {
#if (id386 || idsparc) && !defined __clang__  // calling convention doesn't need conversion in some cases
#ifndef NO_VM_COMPILED
        if (compiled)
            r = VM_CallCompiled(this, (int *)&callnum);
        else
#endif
            r = VM_CallInterpreted(this, (int *)&callnum);
#else
        struct {
            int callnum;
            int args[MAX_VMMAIN_ARGS - 1];
        } a;
        va_list ap;

        a.callnum = callnum;
        va_start(ap, callnum);
        for (unsigned i = 0; i < ARRAY_LEN(a.args); i++) a.args[i] = va_arg(ap, int);
        va_end(ap);

#ifndef NO_VM_COMPILED
        if (compiled)
            r = VM_CallCompiled(this, &a.callnum);
        else
#endif
            r = VM_CallInterpreted(this, &a.callnum);
#endif
    }
    --callLevel;

    if (!oldVM) currentVM = oldVM;

    return r;
}

void vm_s::LoadSymbols(void)
{
    // don't load symbols if not developer
    if (!com_developer->integer)
        return;

    char name[MAX_QPATH];
    COM_StripExtension(name, name, sizeof(name));

    char symfile[MAX_QPATH];
    Com_sprintf(symfile, sizeof(symfile), "vm/%s.map", name);

    union {
        char *c;
        void *v;
    } mapfile;

    FS_ReadFile(symfile, &mapfile.v);
    if (!mapfile.c)
    {
        Com_Printf("Couldn't load symbol file: %s\n", symfile);
        return;
    }

    int numInstructions = instructionCount;

    // parse the symbols
    char* text_p = mapfile.c;
    vmSymbol_t** prev = &symbols;
    int count = 0;

    while (1)
    {
        char* token = COM_Parse(&text_p);
        if (!token[0])
        {
            break;
        }

        int segment = ParseHex(token);
        if (segment)
        {
            COM_Parse(&text_p);
            COM_Parse(&text_p);
            continue;  // only load code segment values
        }

        token = COM_Parse(&text_p);
        if (!token[0])
        {
            Com_Printf("WARNING: incomplete line at end of file\n");
            break;
        }
        int value = ParseHex(token);

        token = COM_Parse(&text_p);
        if (!token[0])
        {
            Com_Printf("WARNING: incomplete line at end of file\n");
            break;
        }

        int n = strlen(token);
        vmSymbol_t* sym = (vmSymbol_t*)Hunk_Alloc(sizeof(*sym) + n, h_high);
        *prev = sym;
        prev = &sym->next;
        sym->next = nullptr;

        // convert value from an instruction number to a code offset
        if (value >= 0 && value < numInstructions)
        {
            value = instructionPointers[value];
        }

        sym->symValue = value;
        Q_strncpyz(sym->symName, token, n + 1);

        count++;
    }

    numSymbols = count;
    Com_Printf("%i symbols parsed from %s\n", count, symfile);
    FS_FreeFile(mapfile.v);
}

