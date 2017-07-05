#pragma once

#include <iostream>

#include "dialog.h"
#include "sys_local.h"
#include "sys_loadlib.h"

#include "../sol/sol.hpp"

using Entry = void (*)(intptr_t (*syscallptr)(intptr_t, ...));
using EntryPoint = intptr_t (QDECL *)(int, ...);
using SysCalls = intptr_t (*)(intptr_t, ...);

class Sys {
public:
    static void ExecuteInstaller(char const*);

    static void SetBinaryPath(char const*);
    static char* BinaryPath();
    static char const* StripAppBundle(char const*);

    static void SetDefaultInstallPath(char const*);
    static char* DefaultInstallPath();
    static char* DefaultAppPath();

    static char* ConsoleInput();
    static char* GetClipboardData();

    static std::string PIDFileName();
    static qboolean WritePIDFile();

    static cpuFeatures_t GetProcessorFeatures();

    static void AnsiColorPrint(char const *msg);
    static void Print(char const *msg);
    static void Error(char const *error, ...);

    static int FileTime(char* path);

    static void* LoadDll(char const*, qboolean);
    static void* LoadGameDll(char const*, EntryPoint*, SysCalls);
    static void UnloadDll(void*);

    static void LoadLua();

    static void Exit(int exitCode);
    static void Quit();

    static void SigHandler(int);
    static void Init();
    static void ParseArgs(int, char**);
    static int main(int, char**);

    static void In_Restart_f();
    static void Installer_f();
    static void Script_f();
    static void ScriptFile_f();

private:
    static sol::state lua;
    static char const* binary_path;
    static char const* install_path;
};


