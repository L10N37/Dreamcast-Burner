#pragma once

#include <filesystem>
#include <string>

struct EmbeddedToolPaths final {
    std::filesystem::path directory;
    std::filesystem::path cdirip;
    std::filesystem::path cdrecord;
    std::filesystem::path cygwin;
    std::filesystem::path growisofs;
    std::filesystem::path dvdMediaInfo;
    std::string error;

    [[nodiscard]] bool Ready() const noexcept {
        return error.empty() &&
            !directory.empty() &&
            !cdirip.empty() &&
            !cdrecord.empty() &&
            !cygwin.empty() &&
            !growisofs.empty() &&
            !dvdMediaInfo.empty();
    }
};

[[nodiscard]] const EmbeddedToolPaths& GetEmbeddedToolPaths();