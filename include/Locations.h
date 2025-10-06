#pragma once
#include <string>
#include <vector>
#include "RE5LocationData.h"

namespace RE5Client {
    
    struct APRE5Entry {
        std::string fileName;
        RE5MemTools::LocationData::APRE5Header header;
        std::string dataJSON;
    };

    void GetAPRE5Entries();

    APRE5Entry GetActiveEntry();
    RE5MemTools::LocationData::Location* GetCurrentLocation(std::string levelARC, int index, std::vector<RE5MemTools::LocationData::Location>&locationData);
}