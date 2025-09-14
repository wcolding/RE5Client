#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iostream>
#include <stdio.h>

#include "MinHook.h"

#include "RE5Text.h"
#include "RE5Items.h"
#include <initializer_list>

template <typename T>
T* ResolvePointer(int baseAddress, const std::initializer_list<int>& offsets)
{
    int curAddr = baseAddress;
    int val = baseAddress;

    for (auto offset : offsets)
    {
        curAddr = val + offset;
        if (curAddr == 0)
            return nullptr;
        val = *reinterpret_cast<int*>(curAddr);
    }

    return reinterpret_cast<T*>(curAddr);
}

template <typename T>
T GetValue(int baseAddress, const std::initializer_list<int>& offsets)
{
    T* ptr = ResolvePointer<T>(baseAddress, offsets);
    return *(T*)ptr;
}

bool debugConsole = true;

std::string getSearchString(int* encrypted)
{
    std::string search;
    for (int i = 0; i < RE5_MAX_STRING_LENGTH; i++)
    {
        if (encrypted[i] == RE5MemTools::Text::StrEnd)
        {
            search = std::format("{}00 00 01 04 ", search, encrypted[i]);
            break;
        } 
        else
        {
            search = std::format("{}{:X} 00 ?? 00 ", search, encrypted[i]);
        }
    }

    return search;
}

typedef void(__stdcall* _WriteText)(void* canvas, int size, int x, int y, int* color, const char* string);
_WriteText WriteText = reinterpret_cast<_WriteText>(0x009E4120);
int textColor = 0xFFFFFFFF;
//0x012340B0

typedef int(__fastcall* _LoadItem)(void* _this, RE5MemTools::mItemSet* itemSet, RE5MemTools::mItemSet* itemSetB, short unk_01, char unk_02, int unk_03, int index, int unk_05);
_LoadItem OriginalLoadItem = nullptr;

int __fastcall HookedLoadItem(void* _this, RE5MemTools::mItemSet* itemSet, RE5MemTools::mItemSet* itemSetB, int unk_01, int unk_02, int unk_03, int index, int unk_05)
{
    if (index == 0)
    {
        auto itemLot = ResolvePointer<char>(0x11B2200, { 0x0, 0x6C, 0x0, 0x4 });
        std::string levelARC = std::string(itemLot, 10).substr(6, 4);
        printf("Level: %s\n\n", levelARC.c_str());
    }

    if (unk_03 != -1)
    {
        if (itemSet->SetType == RE5MemTools::RE5SetType::Suitcase)
        {
            printf("Suitcase item discovered\n");
            itemSet->AutoPosition = true;
            itemSet->NoCheckOba = true;
            RE5MemTools::Item::SetItem(itemSet, RE5MemTools::RE5Item::FlashGrenade, -1);
            //itemSet->SetLotNo = -1;
            //itemSet->mPosition.z -= 100;
        }
        else {
            itemSet->SetType = static_cast<unsigned short>(RE5MemTools::RE5SetType::Spawn);
            RE5MemTools::Item::SetItem(itemSet, RE5MemTools::RE5Item::FlashGrenade, 1);
        }
        
        itemSet->EffType = 1;

        //itemSet->SetLotNo = -1;
    }

    printf("0: %X\n", (int)itemSetB);
    printf("1: %X\n", (int)unk_01);
    printf("2: %X\n", (int)unk_02);
    printf("3: %X\n", (int)unk_03);
    printf("Index: %d\n", (int)index);
    printf("5: %X\n", (int)unk_05);

    auto retVal = OriginalLoadItem(_this, itemSet, itemSetB, unk_01, unk_02, unk_03, index, unk_05);
    printf("Return value: %X\n\n", retVal);
    return retVal;
}

int* menuPtrA; int* menuPtrB; int* menuPtrC;

typedef HRESULT(APIENTRY* _endScene)(void* pDevice);
_endScene OriginalEndScene = nullptr;

long __stdcall HookedEndScene(void* pDevice){
    printf("EndScene called\n");
    //printf(std::format("Menu obj: {:8X}\n", *menuPtrC).c_str());
    //
    WriteText(reinterpret_cast<void*>(*menuPtrC), 32, 400, 40, &textColor, "Testy westy");
    //__asm push 0xFFFFFFFF;
    return OriginalEndScene(pDevice);
}

int SetupHooks()
{
    if (MH_Initialize() != MH_OK)
    {
        printf("Couldn't initialize MinHook\n");
        return 1;
    }

    printf("Initialized MinHook!\n");
    int dx9base = reinterpret_cast<int>(GetModuleHandleA("d3d9.dll"));
    printf(std::format("d3d9.dll address: {:8X}\n", dx9base).c_str());

    LPVOID InjectPoint = reinterpret_cast<LPVOID>(dx9base + 0x648F0);

    if (MH_CreateHook(InjectPoint, &HookedEndScene, reinterpret_cast<LPVOID*>(&OriginalEndScene)) != MH_OK)
    {
        printf("Couldn't create EndScene hook\n");
        MH_Uninitialize();
        return 1;
    }

    printf("Created EndScene hook!\n");
    printf(std::format("Original EndScene: {:8X}\n", (int)OriginalEndScene).c_str());

    if (MH_EnableHook(InjectPoint) != MH_OK)
    {
        printf("Couldn't enable EndScene hook\n");
        MH_Uninitialize();
        return 1;
    }

    printf("Enabled EndScene hook!\n");
    return 0;
}

DWORD WINAPI ModThread(LPVOID hModule)
{
    FILE* pFile = nullptr;
    if (debugConsole)
    {
        AllocConsole();
        freopen_s(&pFile, "CONOUT$", "w", stdout);
    }
    else
        freopen_s(&pFile, "RE5Client_log.txt", "w", stdout);



    while (true)
    {
        if (GetAsyncKeyState(VK_HOME) & 0x01)
        {

            menuPtrA = reinterpret_cast<int*>(0x012340B0);
            menuPtrB = reinterpret_cast<int*>(*menuPtrA + 0x2C);
            menuPtrC = reinterpret_cast<int*>(*menuPtrB + 0x08);
            if (SetupHooks() != 0)
            {
                if (debugConsole)
                    FreeConsole();

                FreeLibraryAndExitThread((HMODULE)hModule, 0);
            }
        }

        if (GetAsyncKeyState(VK_PRIOR) & 0x01)
        {
            if (MH_Initialize() != MH_OK)
            {
                printf("Couldn't initialize MinHook\n");
                return 1;
            }

            printf("Initialized MinHook!\n");

            LPVOID InjectPoint = reinterpret_cast<LPVOID>(0x7BE550);

            if (MH_CreateHook(InjectPoint, &HookedLoadItem, reinterpret_cast<LPVOID*>(&OriginalLoadItem)) != MH_OK)
            {
                printf("Couldn't create LoadItem hook\n");
                MH_Uninitialize();
                return 1;
            }

            printf("Created LoadItem hook!\n");
            printf(std::format("Original LoadItem: {:8X}\n", (int)OriginalLoadItem).c_str());

            if (MH_EnableHook(InjectPoint) != MH_OK)
            {
                printf("Couldn't enable LoadItem hook\n");
                MH_Uninitialize();
                return 1;
            }

            printf("Enabled LoadItem hook!\n");
            return 0;
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
    std::string inputString = "BENCHMARK";
    int* array = RE5MemTools::Text::EncryptString(inputString);
    std::string searchString = getSearchString(array);
    printf("String: %s\n", inputString.c_str());
    printf("Search bytes: %s\n\n", searchString.c_str());

    MH_Uninitialize();

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