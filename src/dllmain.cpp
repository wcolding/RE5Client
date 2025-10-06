#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iostream>
#include <stdio.h>

#include "Locations.h"
#include "Hooks.h"

bool debugConsole = true;

//std::string getSearchString(int* encrypted)
//{
//    std::string search;
//    for (int i = 0; i < RE5_MAX_STRING_LENGTH; i++)
//    {
//        if (encrypted[i] == RE5MemTools::Text::StrEnd)
//        {
//            search = std::format("{}00 00 01 04 ", search, encrypted[i]);
//            break;
//        } 
//        else
//        {
//            search = std::format("{}{:X} 00 ?? 00 ", search, encrypted[i]);
//        }
//    }
//
//    return search;
//}
//
//typedef void(__stdcall* _WriteText)(void* canvas, int size, int x, int y, int* color, const char* string);
//_WriteText WriteText = reinterpret_cast<_WriteText>(0x009E4120);
//int textColor = 0xFFFFFFFF;
////0x012340B0
//
//int* itemLotStart = reinterpret_cast<int*>(0x011B2200);
//
//int* menuPtrA; int* menuPtrB; int* menuPtrC;
//
//typedef HRESULT(APIENTRY* _endScene)(void* pDevice);
//_endScene OriginalEndScene = nullptr;
//
//long __stdcall HookedEndScene(void* pDevice){
//    printf("EndScene called\n");
//    //printf(std::format("Menu obj: {:8X}\n", *menuPtrC).c_str());
//    //
//    WriteText(reinterpret_cast<void*>(*menuPtrC), 32, 400, 40, &textColor, "Testy westy");
//    //__asm push 0xFFFFFFFF;
//    return OriginalEndScene(pDevice);
//}

DWORD WINAPI ModThread(LPVOID hModule)
{
    FILE* pFile = nullptr;
    if (debugConsole) {
        AllocConsole();
        freopen_s(&pFile, "CONOUT$", "w", stdout);
    }
    else
        freopen_s(&pFile, "RE5Client_log.txt", "w", stdout);

    RE5Client::GetAPRE5Entries();

    while (true)
    {
        //if (GetAsyncKeyState(VK_HOME) & 0x01)
        
        if (GetAsyncKeyState(VK_NEXT) & 0x01)
        {
            RE5Client::HookLoadARC();
        }

        if (GetAsyncKeyState(VK_END) & 0x01)
        {
            break;
        }
    }
    
    //while (true) {}

    // 009E4120 - actual nice string func??? ->009E3FE0  ->007DD790
    // text ptr, color ptr, y pos, x pos, font size
    // 
    //+3DB2B0 - called to draw unselected / white text?
    // 
    // 01 04 ends string
    /*std::string inputString = "BENCHMARK";
    int* array = RE5MemTools::Text::EncryptString(inputString);
    std::string searchString = getSearchString(array);
    printf("String: %s\n", inputString.c_str());
    printf("Search bytes: %s\n\n", searchString.c_str());*/

    RE5Client::EndMinHook();

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