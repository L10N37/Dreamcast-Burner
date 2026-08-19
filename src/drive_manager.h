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
    std::string cdrecordDevice; // Legacy field name: RetroBeam/libscg address.

    // RetroBeam capability fingerprint used by the Advanced Settings panel.
    bool retrobeamCapabilitiesKnown = false;
    bool burnFreeSupported = false;
    bool forceSpeedSupported = false;

    bool realTimeStreamingKnown = false;
    bool realTimeStreamingCurrent = false;
    bool realTimeStreamingPersistent = false;
    bool streamRecordingSupported = false;
    bool getPerformanceWriteSpeedSupported = false;
    bool modePage2AWriteSpeedSupported = false;
    bool setCdSpeedSupported = false;
    bool readBufferCapacitySupported = false;

    bool opcDescriptorCountKnown = false;
    std::uint32_t opcDescriptorCount = 0;

    bool driveBufferCapacityKnown = false;
    std::uint32_t driveBufferCapacityBytes = 0;
    std::uint32_t driveBufferAvailableBytes = 0;

    std::string advancedCapabilityMessage;

    // Values used to mirror cdrtools/RetroBeam Windows SPTI bus numbering.
    bool scsiAddressValid = false;
    std::uint16_t scsiBusKey = 0;
    std::uint8_t scsiTarget = 0;
    std::uint8_t scsiLun = 0;

    [[nodiscard]] std::string DisplayName() const;
};

[[nodiscard]] std::vector<OpticalDrive> EnumerateOpticalDrives();
