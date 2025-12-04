#include "Hooks.h"
#include "MinHook.h"
#include "Locations.h"

#include <string>
#include <format>
#include <map>

bool minhookInitialized = false;

std::map<int, int> CurrentLocationsMap;

#pragma region LoadARC
// Original function definition and pointer to new address
typedef void(__fastcall* _LoadARC)(ARCFileEntry* file);
_LoadARC OriginalLoadARC = nullptr;

// Function to execute after loading ARC file
char partialFile[6] = "     ";
char fileSection[5] = "    ";
std::string levelARC = "";
std::vector<RE5MemTools::LocationData::Location> locationData;
void __fastcall HookedLoadARC(ARCFileEntry* file)
{
    memcpy(&partialFile, &file->name, 5);
    if (strcmp(partialFile, "stage") == 0)
    {
        std::string fileName = std::string(file->name);

        if ((fileName.find("_item") != std::string::npos) || (fileName.find("_Item") != std::string::npos))
        {
            printf("Loaded ARC file \"%s\"\n", file->name);
            levelARC = std::string(file->name, 10).substr(6, 4);
            printf("Level: %s\n", levelARC.c_str());

            // Refresh APRE5 file if uncompressed
            bool apre5IsCompressed = RE5Client::GetActiveEntry().header.compressed;
            if (apre5IsCompressed)
            {
                printf("Loaded APRE5 file is compressed\n");
            }
            else
            {
                printf("Loaded APRE5 file is uncompressed\n");
                RE5Client::APRE5Entry newEntry;
                newEntry.fileName = RE5Client::GetActiveEntry().fileName;

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
                    RE5MemTools::Item::SetItem(&curEntry->mpInfo->mItemSet, static_cast<RE5MemTools::RE5Item>(curLocData->item), curLocData->qty);
                    printf("Setting location %i to %i (%i)\n", curLocData->id, curLocData->item, curLocData->qty);
                }
            }
        }
    }

    OriginalLoadARC(file);
}

// Execute hook
bool RE5Client::HookLoadARC()
{
    if (!minhookInitialized)
        return false;

    LPVOID InjectPoint = reinterpret_cast<LPVOID>(0x44F380);

    if (MH_CreateHook(InjectPoint, &HookedLoadARC, reinterpret_cast<LPVOID*>(&OriginalLoadARC)) != MH_OK)
    {
        printf("Couldn't create LoadARC hook\n");
        MH_Uninitialize();
        return false;
    }

    printf("Created LoadARC hook!\n");
    printf(std::format("Original LoadARC: {:8X}\n", (int)OriginalLoadARC).c_str());

    if (MH_EnableHook(InjectPoint) != MH_OK)
    {
        printf("Couldn't enable LoadARC hook\n");
        MH_Uninitialize();
        return false;
    }

    printf("Enabled LoadARC hook!\n");
    return true;
}
#pragma endregion LoadARC

#pragma region InstantiatePickup
// Original function definition and pointer to new address
typedef int(__fastcall* _InstantiatePickup)(void* _this, int index, void* copyTarget);
_InstantiatePickup OriginalInstantiatePickup = nullptr;

// Function to execute before instantiating Pickup
int __fastcall HookedInstantiatePickup(void* _this, int index, void* copyTarget)
{
    if (index == 0) {
        CurrentLocationsMap.clear();
    }

    //printf("%x, %x, %x\n", _this, index, copyTarget);
    int locID = RE5Client::GetLocationID(levelARC, index);
    if (locID != -1) {
        CurrentLocationsMap[reinterpret_cast<int>(_this)] = locID;
        printf("Mapped location id %i to item at %x\n", locID, _this);
    }
    return OriginalInstantiatePickup(_this, index, copyTarget);
}

// Execute hook
bool RE5Client::HookInstantiatePickup()
{
    if (!minhookInitialized)
        return false;

    LPVOID InjectPoint = reinterpret_cast<LPVOID>(0xA784B0);

    if (MH_CreateHook(InjectPoint, &HookedInstantiatePickup, reinterpret_cast<LPVOID*>(&OriginalInstantiatePickup)) != MH_OK)
    {
        printf("Couldn't create InstantiatePickup hook\n");
        MH_Uninitialize();
        return false;
    }

    printf("Created InstantiatePickup hook!\n");
    printf(std::format("Original InstantiatePickup: {:8X}\n", (int)OriginalInstantiatePickup).c_str());

    if (MH_EnableHook(InjectPoint) != MH_OK)
    {
        printf("Couldn't enable InstantiatePickup hook\n");
        MH_Uninitialize();
        return false;
    }

    printf("Enabled InstantiatePickup hook!\n");
    return true;
}
#pragma endregion InstantiatePickup

#pragma region Pickup
// Original function definition and pointer to new address
typedef void(*_Pickup)();
_Pickup OriginalPickup = nullptr;

int itemAddr;
int pickupReturn = 0xC7F8B9;
int pickupAPLoc = 0;

// Function to execute before Pickup
__declspec(naked) void HookedPickup()
{
    _asm {
        //pushad;
        mov itemAddr, eax;
    }

    if (CurrentLocationsMap.contains(itemAddr)) {
        pickupAPLoc = CurrentLocationsMap[itemAddr];
        printf("Checked item at APlocation %i\n", pickupAPLoc);
    }

    _asm jmp pickupReturn;
}

// Execute hook
bool RE5Client::HookPickup()
{
    if (!minhookInitialized)
        return false;

    LPVOID InjectPoint = reinterpret_cast<LPVOID>(0xC7F8B2);

    if (MH_CreateHook(InjectPoint, &HookedPickup, reinterpret_cast<LPVOID*>(&OriginalPickup)) != MH_OK)
    {
        printf("Couldn't create Pickup hook\n");
        MH_Uninitialize();
        return false;
    }

    printf("Created Pickup hook!\n");
    printf(std::format("Original Pickup: {:8X}\n", (int)OriginalPickup).c_str());

    if (MH_EnableHook(InjectPoint) != MH_OK)
    {
        printf("Couldn't enable Pickup hook\n");
        MH_Uninitialize();
        return false;
    }

    printf("Enabled Pickup hook!\n");
    return true;
}
#pragma endregion Pickup

void RE5Client::StartMinHook()
{
    minhookInitialized = (MH_Initialize() == MH_OK);
    if (minhookInitialized)
        printf("Initialized MinHook!\n");
    else
        printf("Unable to initialize MinHook!\n");
}

void RE5Client::EndMinHook()
{
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();
}