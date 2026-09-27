#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <cstring>
#include <iostream>

struct SarcHeader {
    char magic[4];       // "SARC"
    uint16_t headerLen;  // Always 0x0014
    uint16_t byteOrder;   // 0xFEFF (Little Endian for 3DS)
    uint32_t fileSize;   // Total archive size
    uint32_t dataOffset; // Where raw file data starts
    uint32_t unknown;
};

struct SfatHeader {
    char magic[4];       // "SFAT"
    uint16_t headerLen;  // Always 0x000C
    uint16_t nodeCount;  // How many files are inside
    uint32_t hashMultiplier;
};

struct SfatNode {
    uint32_t nameHash;
    uint32_t fileFlags;  // Tells if the node points to a string name
    uint32_t nodeOffset; // Offset relative to the Data Block start
    uint32_t nodeEnd;    // End offset of file data
};

class SarcArchive {
public:
    std::vector<uint8_t> rawData;

    bool load(const std::vector<uint8_t>& data) {
        if (data.size() < sizeof(SarcHeader)) return false;
        rawData = data;
        
        // Quick verification of the magic identifier
        return (std::memcmp(rawData.data(), "SARC", 4) == 0);
    }

    // Searches the archive for a file name and returns a pointer to its raw data
    std::vector<uint8_t> get_file(const std::string& internalPath) {
        const SarcHeader* header = reinterpret_cast<const SarcHeader*>(rawData.data());
        
        // SFAT always starts immediately after the 20-byte SARC header
        uint32_t sfatOffset = header->headerLen;
        const SfatHeader* sfat = reinterpret_cast<const SfatHeader*>(rawData.data() + sfatOffset);
        
        if (std::memcmp(sfat->magic, "SFAT", 4) != 0) return {};

        const SfatNode* nodes = reinterpret_cast<const SfatNode*>(rawData.data() + sfatOffset + sfat->headerLen);
        
        // Calculate where the String Name Table (SFNT) lives
        uint32_t sfntOffset = sfatOffset + sfat->headerLen + (sfat->nodeCount * sizeof(SfatNode));
        const char* nameTable = reinterpret_cast<const char*>(rawData.data() + sfntOffset + 8); // Skip SFNT header

        // Loop through all file nodes inside the SARC archive
        for (int i = 0; i < sfat->nodeCount; ++i) {
            uint32_t nameOffset = (nodes[i].fileFlags & 0x00FFFFFF) * 4;
            std::string filename(nameTable + nameOffset);

            // Match found! Extract raw binary chunk
            if (filename == internalPath) {
                uint32_t start = header->dataOffset + nodes[i].nodeOffset;
                uint32_t size = nodes[i].nodeEnd - nodes[i].nodeOffset;
                
                return std::vector<uint8_t>(rawData.begin() + start, rawData.begin() + start + size);
            }
        }
        
        std::cerr << "[SARC Warning] File not found in archive: " << internalPath << std::endl;
        return {};
    }
};
