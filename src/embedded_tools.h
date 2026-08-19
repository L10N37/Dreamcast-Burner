#pragma once

#include <filesystem>
#include <string>

struct EmbeddedToolPaths final {
    std::filesystem::path directory;
    std::filesystem::path cdirip;
    std::filesystem::path retrobeam;
    std::filesystem::path growisofs;
    std::filesystem::path dvdMediaInfo;
    std::filesystem::path abgx360;
    std::string error;

    [[nodiscard]] bool Ready() const noexcept {
        return error.empty() &&
            !directory.empty() &&
            !cdirip.empty() &&
            !retrobeam.empty() &&
            !growisofs.empty() &&
            !dvdMediaInfo.empty() &&
            !abgx360.empty();
    }
};

[[nodiscard]] const EmbeddedToolPaths& GetEmbeddedToolPaths();
