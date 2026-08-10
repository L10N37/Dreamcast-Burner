#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct WriteSpeed final {
    std::uint32_t kilobytesPerSecond = 0;
    float cdMultiplier = 0.0F;
    bool exactForWholeMedia = false;
    std::string rotation;
};

struct OpticalDrive final {
    std::wstring rootPath;
    std::wstring devicePath;
    std::string vendor;
    std::string product;
    std::string firmware;
    std::string bus;
    bool mediaPresent = false;
    bool blankMediaKnown = false;
    bool blankMedia = false;
    bool cdWriteCapabilityKnown = false;
    bool canWriteCdR = false;
    std::uint16_t currentProfile = 0;
    std::string mediaDescription;
    std::vector<WriteSpeed> writeSpeeds;
    std::string speedQueryMessage;
    std::string cdrecordDevice;

    // Values used to mirror cdrtools' Windows SPTI bus numbering.
    bool scsiAddressValid = false;
    std::uint16_t scsiBusKey = 0;
    std::uint8_t scsiTarget = 0;
    std::uint8_t scsiLun = 0;

    [[nodiscard]] std::string DisplayName() const;
};

[[nodiscard]] std::vector<OpticalDrive> EnumerateOpticalDrives();
