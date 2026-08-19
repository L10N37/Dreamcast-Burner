#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

enum class BurnerMaxStatus {
    AlreadyEnabled,
    Enabled,
    Unsupported,
    IoError,
};

struct BurnerMaxResult final {
    BurnerMaxStatus status = BurnerMaxStatus::IoError;
    std::string backend;
    std::string message;
    std::uint32_t layerBoundary = 0;
    std::vector<std::uint16_t> layer0Registers;
    std::vector<std::uint16_t> layer1Registers;

    [[nodiscard]] bool Success() const noexcept {
        return status == BurnerMaxStatus::AlreadyEnabled ||
               status == BurnerMaxStatus::Enabled;
    }
};

using BurnerMaxLogCallback =
    std::function<void(const std::string&)>;

// Applies the volatile BurnerMAX payload used by compatible MediaTek-based
// DVD writers. No disc sectors are written. The result is accepted only if
// the drive subsequently reports the XGD3 layer boundary (2133520).
[[nodiscard]] BurnerMaxResult EnableBurnerMax(
    const std::wstring& opticalDriveRoot,
    const BurnerMaxLogCallback& log);
