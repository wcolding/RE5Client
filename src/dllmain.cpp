#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iostream>
#include <stdio.h>
#include "Locations.h"

bool debugConsole = true;

DWORD WINAPI ModThread(LPVOID hModule)
{
    FILE* pFile = nullptr;
    if (debugConsole) {
        AllocConsole();
        freopen_s(&pFile, "CONOUT$", "w", stdout);
    }
    else
        freopen_s(&pFile, "RE5Client_log.txt", "w", stdout);

    auto entries = RE5Client::GetAPRE5Entries();
  
    for (auto entry : entries) {
        printf("Seed found: %s\nSlot name: %s\n\n", entry.header.seedName, entry.header.slotName);
    }

    if (debugConsole)
        FreeConsole();

    FreeLibraryAndExitThread((HMODULE)hModule, 0);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved
)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    {
        DWORD threadID = 0;
        CreateThread(NULL, 0, ModThread, hModule, 0, &threadID);
        
        break;
    }
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}