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

     std::vector<APRE5Entry> GetAPRE5Entries();

     std::vector<RE5MemTools::LocationData::Location> GetCurrentLocations(std::string levelARC, int index, std::vector<RE5MemTools::LocationData::Location>& locationData);
}