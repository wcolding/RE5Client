#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iostream>
#include <stdio.h>
#include "Locations.h"

#include "MinHook.h"

#include "RE5Text.h"
#include "RE5Items.h"
#include <initializer_list>

std::vector<RE5Client::APRE5Entry> APRE5Entries;
int activeAPRE5Entry = 0;
std::string levelARC = ""; 
std::vector<RE5MemTools::LocationData::Location> locationData;

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

struct RE5Array
{
    int count;
    int max;
    int unk;
    void* data;
};

struct RE5ItemEntry
{
    void* vTable;
    int mID;
    RE5MemTools::RE5String* mInfoClass;
    RE5MemTools::RE5String* mUnitClass;
    RE5MemTools::mpInfo* mpInfo;
};

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

struct ARCFileEntry {
    void* vTable;
    char name[24];
};

typedef void(__fastcall* _LoadARC)(ARCFileEntry* file);
_LoadARC OriginalLoadARC = nullptr;

int* itemLotStart = reinterpret_cast<int*>(0x011B2200);


char partialFile[6] = "     ";
char fileSection[5] = "    ";
void __fastcall HookedLoadARC(ARCFileEntry* file)
{
    memcpy(&partialFile, &file->name, 5);
    if (strcmp(partialFile, "stage") == 0)
    {
        void* section = reinterpret_cast<char*>(reinterpret_cast<int>(file->name) + 21);
        memcpy(&fileSection, section, 4);
        if (strcmp(fileSection, "item") == 0)
        {
            printf("Loaded ARC file \"%s\"\n", file->name);
            levelARC = std::string(file->name, 10).substr(6, 4);
            printf("Level: %s\n", levelARC.c_str());

            // Refresh APRE5 file if uncompressed
            bool apre5IsCompressed = APRE5Entries[activeAPRE5Entry].header.compressed;
            if (apre5IsCompressed)
            {
                printf("Loaded APRE5 file is compressed\n");
            }
            else
            {
                printf("Loaded APRE5 file is uncompressed\n");
                RE5Client::APRE5Entry newEntry;
                newEntry.fileName = APRE5Entries[activeAPRE5Entry].fileName;

                int result = RE5MemTools::LocationData::DecompressJSONFromFile(newEntry.fileName, newEntry.dataJSON, newEntry.header);
                if (result == LOC_DATA_OK) {
                    locationData.clear();
                    locationData = RE5MemTools::LocationData::GetLocationData(newEntry.dataJSON);
                    printf("APRE5 file %s refreshed\n", newEntry.fileName.c_str());
                }
            }

            auto itemLot = ResolvePointer<RE5Array>(0x11B2200, { 0x0, 0x6C, 0x0, 0x64 });
            printf("Items count: %i\n", itemLot->count);
            printf("Data addr: %x\n", itemLot->data);
            for (int i = 0; i < itemLot->count; i++)
            {
                auto curEntry = reinterpret_cast<RE5ItemEntry*>(*reinterpret_cast<int**>(reinterpret_cast<int>(itemLot->data) + 4 * i));
                auto curLocData = RE5Client::GetCurrentLocation(levelARC, i, locationData);
                if (curLocData != nullptr)
                {
                    curEntry->mpInfo->mItemSet.ItemId = curLocData->item;
                    curEntry->mpInfo->mItemSet.ItemNum = curLocData->qty;
                    printf("Setting location %i to %i (%i)\n", curLocData->id, curLocData->item, curLocData->qty);
                }
            }
        }
    }
    OriginalLoadARC(file);
}

typedef int(__fastcall* _LoadItem)(void* _this, RE5MemTools::mItemSet* itemSet, RE5MemTools::mItemSet* itemSetB, short unk_01, char unk_02, int unk_03, int index, int unk_05);
_LoadItem OriginalLoadItem = nullptr;

int __fastcall HookedLoadItem(void* _this, RE5MemTools::mItemSet* itemSet, RE5MemTools::mItemSet* itemSetB, int unk_01, int unk_02, int unk_03, int index, int unk_05)
{
    if (index == 0)
    {
        auto itemLot = ResolvePointer<char>(0x11B2200, { 0x0, 0x6C, 0x0, 0x4 });
        levelARC = std::string(itemLot, 10).substr(6, 4);
        printf("Level: %s\n\n", levelARC.c_str());
        bool apre5IsCompressed = APRE5Entries[activeAPRE5Entry].header.compressed;
        if (apre5IsCompressed)
        {
            printf("Loaded APRE5 file is compressed\n");
        } 
        else
        {
            printf("Loaded APRE5 file is uncompressed\n");
            // Refresh APRE5 file if uncompressed (hot reload)
            RE5Client::APRE5Entry newEntry;// = APRE5Entries[activeAPRE5Entry];
            newEntry.fileName = APRE5Entries[activeAPRE5Entry].fileName;
    
            int result = RE5MemTools::LocationData::DecompressJSONFromFile(newEntry.fileName, newEntry.dataJSON, newEntry.header);
            if (result == LOC_DATA_OK) {
                //printf("Loaded json string: %s\n\n", newEntry.dataJSON.c_str());
                locationData.clear();
                locationData = RE5MemTools::LocationData::GetLocationData(newEntry.dataJSON);
                printf("APRE5 file %s refreshed\n", newEntry.fileName.c_str());

                //currentLocations = RE5Client::GetCurrentLocations(levelARC, index, locationData);
               // printf("%i locations matched from file\n\n", currentLocations.size());
            }
        }
    }

    // Load arc files +328D20
    // populate in game data from file +3F3C60
    // populate continued? +3F2D80
    // +4F380


    RE5MemTools::LocationData::Location* currentLocation = RE5Client::GetCurrentLocation(levelARC, index, locationData);
    //printf("Cur loc ptr: %i\n", (int)currentLocation);
    if (unk_03 != -1)
    {
        if (itemSet->SetType == RE5MemTools::RE5SetType::Suitcase)
        {
            printf("Suitcase item discovered\n");
            itemSet->AutoPosition = true;
            itemSet->NoCheckOba = true;
            if (currentLocation != nullptr)
                RE5MemTools::Item::SetItem(itemSet, static_cast<RE5MemTools::RE5Item>(currentLocation->item), currentLocation->qty);
            //itemSet->SetLotNo = -1;
            //itemSet->mPosition.z -= 100;
        }
        else {
            itemSet->SetType = static_cast<unsigned short>(RE5MemTools::RE5SetType::Spawn);

            if (currentLocation != nullptr)
                RE5MemTools::Item::SetItem(itemSet, static_cast<RE5MemTools::RE5Item>(currentLocation->item), currentLocation->qty);
        }
        
        itemSet->EffType = 1;

        //itemSet->SetLotNo = -1;
    }

    /*printf("0: %X\n", (int)itemSetB);
    printf("1: %X\n", (int)unk_01);
    printf("2: %X\n", (int)unk_02);
    printf("3: %X\n", (int)unk_03);
    printf("Index: %d\n", (int)index);
    printf("5: %X\n", (int)unk_05);*/

    auto retVal = OriginalLoadItem(_this, itemSet, itemSetB, unk_01, unk_02, unk_03, index, unk_05);
    printf("Return value: %X\n\n", retVal);


    if (currentLocation != nullptr)
    {
        int* apLocID = reinterpret_cast<int*>(retVal + 0x1C);
        *apLocID = currentLocation->id;
    }

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
    if (debugConsole) {
        AllocConsole();
        freopen_s(&pFile, "CONOUT$", "w", stdout);
    }
    else
        freopen_s(&pFile, "RE5Client_log.txt", "w", stdout);

    APRE5Entries = RE5Client::GetAPRE5Entries();

    for (auto entry : APRE5Entries) {
        printf("Seed found: %s\nSlot name: %s\n\n", entry.header.seedName, entry.header.slotName);
    }

    if (!APRE5Entries.empty())
        activeAPRE5Entry = 0; // temporary autoselect

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

        if (GetAsyncKeyState(VK_NEXT) & 0x01)
        {
            if (MH_Initialize() != MH_OK)
            {
                printf("Couldn't initialize MinHook\n");
                return 1;
            }

            printf("Initialized MinHook!\n");

            LPVOID InjectPoint = reinterpret_cast<LPVOID>(0x44F380);

            if (MH_CreateHook(InjectPoint, &HookedLoadARC, reinterpret_cast<LPVOID*>(&OriginalLoadARC)) != MH_OK)
            {
                printf("Couldn't create LoadARC hook\n");
                MH_Uninitialize();
                return 1;
            }

            printf("Created LoadARC hook!\n");
            printf(std::format("Original LoadARC: {:8X}\n", (int)OriginalLoadARC).c_str());

            if (MH_EnableHook(InjectPoint) != MH_OK)
            {
                printf("Couldn't enable LoadARC hook\n");
                MH_Uninitialize();
                return 1;
            }

            printf("Enabled LoadARC hook!\n");
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