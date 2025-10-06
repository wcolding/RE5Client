#pragma once
#include "RE5Items.h"
#include <initializer_list>

struct ARCFileEntry {
    void* vTable;
    char name[24];
};

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

namespace RE5Client {
   
    bool HookLoadARC();
    void EndMinHook();
}