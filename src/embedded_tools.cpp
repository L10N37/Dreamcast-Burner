#include "embedded_tools.h"

#include <windows.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

#include "resource.h"

namespace {

namespace fs = std::filesystem;

[[nodiscard]] std::string Win32Code(const DWORD error) {
    return "Windows error " + std::to_string(error);
}

[[nodiscard]] bool WriteResourceToFile(
    const int resourceId,
    const fs::path& destination,
    std::string& error) {
    HMODULE module = GetModuleHandleW(nullptr);
    if (module == nullptr) {
        error = "Could not access the Retro Burner executable resources.";
        return false;
    }

    HRSRC resource = FindResourceW(
        module,
        MAKEINTRESOURCEW(resourceId),
        RT_RCDATA);
    if (resource == nullptr) {
        error = "Embedded backend resource " + std::to_string(resourceId) +
            " is missing (" + Win32Code(GetLastError()) + ").";
        return false;
    }

    const DWORD size = SizeofResource(module, resource);
    HGLOBAL loaded = LoadResource(module, resource);
    const void* bytes = loaded != nullptr ? LockResource(loaded) : nullptr;
    if (loaded == nullptr || bytes == nullptr || size == 0) {
        error = "Embedded backend resource " + std::to_string(resourceId) +
            " could not be read.";
        return false;
    }

    std::ofstream stream(destination, std::ios::binary | std::ios::trunc);
    if (!stream) {
        error = "Could not create temporary backend file: " +
            destination.string();
        return false;
    }

    stream.write(
        static_cast<const char*>(bytes),
        static_cast<std::streamsize>(size));
    stream.close();

    if (!stream) {
        error = "Could not finish writing temporary backend file: " +
            destination.string();
        return false;
    }
    return true;
}

class EmbeddedToolStore final {
public:
    EmbeddedToolStore() {
        std::array<wchar_t, 32768> tempPath{};
        const DWORD length = GetTempPathW(
            static_cast<DWORD>(tempPath.size()),
            tempPath.data());
        if (length == 0 || length >= tempPath.size()) {
            paths_.error = "Windows could not locate the temporary directory.";
            return;
        }

        paths_.directory =
            fs::path(tempPath.data()) /
            L"RetroBurner" /
            std::to_wstring(GetCurrentProcessId());

        std::error_code ec;
        fs::create_directories(paths_.directory, ec);
        if (ec) {
            paths_.error =
                "Could not create Retro Burner's temporary backend directory: " +
                ec.message();
            return;
        }

        paths_.cdirip = paths_.directory / L"cdirip.exe";
        paths_.cdrecord = paths_.directory / L"cdrecord.exe";
        paths_.cygwin = paths_.directory / L"cygwin1.dll";
        paths_.growisofs = paths_.directory / L"growisofs.exe";
        paths_.dvdMediaInfo = paths_.directory / L"dvd+rw-mediainfo.exe";

        if (!WriteResourceToFile(
                IDR_BIN_CDIRIP, paths_.cdirip, paths_.error) ||
            !WriteResourceToFile(
                IDR_BIN_CDRECORD, paths_.cdrecord, paths_.error) ||
            !WriteResourceToFile(
                IDR_BIN_CYGWIN, paths_.cygwin, paths_.error) ||
            !WriteResourceToFile(
                IDR_BIN_GROWISOFS, paths_.growisofs, paths_.error) ||
            !WriteResourceToFile(
                IDR_BIN_DVD_MEDIAINFO, paths_.dvdMediaInfo, paths_.error)) {
            std::error_code cleanupError;
            fs::remove_all(paths_.directory, cleanupError);
            return;
        }
    }

    ~EmbeddedToolStore() {
        if (paths_.directory.empty()) {
            return;
        }
        std::error_code ec;
        fs::remove_all(paths_.directory, ec);
    }

    EmbeddedToolStore(const EmbeddedToolStore&) = delete;
    EmbeddedToolStore& operator=(const EmbeddedToolStore&) = delete;

    [[nodiscard]] const EmbeddedToolPaths& Paths() const noexcept {
        return paths_;
    }

private:
    EmbeddedToolPaths paths_;
};

} // namespace

const EmbeddedToolPaths& GetEmbeddedToolPaths() {
    static const EmbeddedToolStore store;
    return store.Paths();
}