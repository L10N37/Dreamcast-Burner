#include "burn_engine.h"
#include "embedded_tools.h"
#include "resource.h"

#include <windows.h>
#include <mmsystem.h>
#include <winioctl.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <regex>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace {

namespace fs = std::filesystem;

constexpr std::uintmax_t kMinimumFreeSpaceMargin = 256ULL * 1024ULL * 1024ULL;
constexpr std::size_t kMaximumLogBytes = 2U * 1024U * 1024U;

struct UniqueHandle final {
    HANDLE value = INVALID_HANDLE_VALUE;

    UniqueHandle() = default;
    explicit UniqueHandle(const HANDLE handle) noexcept : value(handle) {}
    ~UniqueHandle() {
        if (value != INVALID_HANDLE_VALUE && value != nullptr) {
            CloseHandle(value);
        }
    }
    UniqueHandle(const UniqueHandle&) = delete;
    UniqueHandle& operator=(const UniqueHandle&) = delete;
    UniqueHandle(UniqueHandle&& other) noexcept : value(other.value) {
        other.value = INVALID_HANDLE_VALUE;
    }
    UniqueHandle& operator=(UniqueHandle&& other) noexcept {
        if (this != &other) {
            if (value != INVALID_HANDLE_VALUE && value != nullptr) {
                CloseHandle(value);
            }
            value = other.value;
            other.value = INVALID_HANDLE_VALUE;
        }
        return *this;
    }
    [[nodiscard]] bool Valid() const noexcept {
        return value != INVALID_HANDLE_VALUE && value != nullptr;
    }
    HANDLE Release() noexcept {
        const HANDLE result = value;
        value = INVALID_HANDLE_VALUE;
        return result;
    }
};

struct TemporaryDirectory final {
    fs::path path;
    ~TemporaryDirectory() {
        if (!path.empty()) {
            std::error_code ignored;
            fs::remove_all(path, ignored);
        }
    }
};

struct ExtractedLayout final {
    bool audioData = false;
    std::vector<fs::path> firstSession;
    std::vector<fs::path> secondSession;
    std::uintmax_t firstSessionBytes = 0;
    std::uintmax_t secondSessionBytes = 0;
    std::string description;
};

[[nodiscard]] std::wstring QuoteArgument(const std::wstring& argument) {
    if (argument.empty()) {
        return L"\"\"";
    }
    if (argument.find_first_of(L" \t\"") == std::wstring::npos) {
        return argument;
    }

    std::wstring result = L"\"";
    std::size_t backslashes = 0;
    for (const wchar_t character : argument) {
        if (character == L'\\') {
            ++backslashes;
            continue;
        }
        if (character == L'\"') {
            result.append(backslashes * 2U + 1U, L'\\');
            result.push_back(L'\"');
            backslashes = 0;
            continue;
        }
        result.append(backslashes, L'\\');
        backslashes = 0;
        result.push_back(character);
    }
    result.append(backslashes * 2U, L'\\');
    result.push_back(L'\"');
    return result;
}

[[nodiscard]] std::wstring BuildCommandLine(
    const fs::path& executable,
    const std::vector<std::wstring>& arguments) {
    std::wstring command = QuoteArgument(executable.wstring());
    for (const std::wstring& argument : arguments) {
        command.push_back(L' ');
        command += QuoteArgument(argument);
    }
    return command;
}

[[nodiscard]] std::string Win32Error(const DWORD error) {
    wchar_t* buffer = nullptr;
    const DWORD length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
            FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        error,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<wchar_t*>(&buffer),
        0,
        nullptr);

    std::string result;
    if (length > 0 && buffer != nullptr) {
        const int bytes = WideCharToMultiByte(
            CP_UTF8, 0, buffer, static_cast<int>(length),
            nullptr, 0, nullptr, nullptr);
        if (bytes > 0) {
            result.resize(static_cast<std::size_t>(bytes));
            WideCharToMultiByte(
                CP_UTF8, 0, buffer, static_cast<int>(length),
                result.data(), bytes, nullptr, nullptr);
            while (!result.empty() &&
                   (result.back() == '\r' || result.back() == '\n' || result.back() == ' ')) {
                result.pop_back();
            }
        }
        LocalFree(buffer);
    }
    if (result.empty()) {
        result = "Windows error " + std::to_string(error);
    }
    return result;
}

[[nodiscard]] fs::path ExecutableDirectory() {
    std::wstring buffer(32768, L'\0');
    const DWORD length = GetModuleFileNameW(
        nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= buffer.size()) {
        return {};
    }
    buffer.resize(length);
    return fs::path(buffer).parent_path();
}

[[nodiscard]] TemporaryDirectory MakeTemporaryDirectory() {
    std::wstring buffer(32768, L'\0');
    const DWORD length = GetTempPathW(
        static_cast<DWORD>(buffer.size()), buffer.data());
    if (length == 0 || length >= buffer.size()) {
        return {};
    }
    buffer.resize(length);
    const fs::path root(buffer);
    const DWORD processId = GetCurrentProcessId();
    const ULONGLONG tick = GetTickCount64();
    for (unsigned attempt = 0; attempt < 100; ++attempt) {
        fs::path candidate = root /
            (L"DreamcastBurner-" + std::to_wstring(processId) + L"-" +
             std::to_wstring(tick) + L"-" + std::to_wstring(attempt));
        std::error_code error;
        if (fs::create_directory(candidate, error)) {
            return TemporaryDirectory{std::move(candidate)};
        }
    }
    return {};
}

using OutputCallback = std::function<void(std::string_view)>;

struct ProcessResult final {
    bool started = false;
    DWORD exitCode = ERROR_GEN_FAILURE;
    std::string error;
};

[[nodiscard]] ProcessResult RunHiddenProcess(
    const fs::path& executable,
    const std::vector<std::wstring>& arguments,
    const fs::path& workingDirectory,
    const std::function<void(const std::string&)>& appendLog,
    const OutputCallback& onLine) {
    SECURITY_ATTRIBUTES security{};
    security.nLength = sizeof(security);
    security.bInheritHandle = TRUE;

    HANDLE readRaw = INVALID_HANDLE_VALUE;
    HANDLE writeRaw = INVALID_HANDLE_VALUE;
    if (!CreatePipe(&readRaw, &writeRaw, &security, 0)) {
        return {false, ERROR_GEN_FAILURE, "Could not create the process output pipe: " +
            Win32Error(GetLastError())};
    }
    UniqueHandle readPipe(readRaw);
    UniqueHandle writePipe(writeRaw);
    SetHandleInformation(readPipe.value, HANDLE_FLAG_INHERIT, 0);

    UniqueHandle nullInput(CreateFileW(
        L"NUL",
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        &security,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr));
    if (!nullInput.Valid()) {
        return {false, ERROR_GEN_FAILURE, "Could not open the child-process input: " +
            Win32Error(GetLastError())};
    }

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = writePipe.value;
    startup.hStdError = writePipe.value;
    startup.hStdInput = nullInput.value;

    PROCESS_INFORMATION process{};
    std::wstring commandLine = BuildCommandLine(executable, arguments);
    const BOOL created = CreateProcessW(
        executable.c_str(),
        commandLine.data(),
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW,
        nullptr,
        workingDirectory.empty() ? nullptr : workingDirectory.c_str(),
        &startup,
        &process);
    if (!created) {
        return {false, ERROR_GEN_FAILURE, "Could not start " +
            executable.filename().string() + ": " + Win32Error(GetLastError())};
    }

    UniqueHandle processHandle(process.hProcess);
    UniqueHandle threadHandle(process.hThread);
    writePipe = UniqueHandle();

    std::array<char, 4096> chunk{};
    std::string pendingLine;
    for (;;) {
        DWORD bytesRead = 0;
        if (!ReadFile(
                readPipe.value,
                chunk.data(),
                static_cast<DWORD>(chunk.size()),
                &bytesRead,
                nullptr) ||
            bytesRead == 0) {
            break;
        }

        const std::string text(chunk.data(), bytesRead);
        appendLog(text);
        pendingLine += text;
        for (;;) {
            const std::size_t separator = pendingLine.find_first_of("\r\n");
            if (separator == std::string::npos) {
                break;
            }
            if (separator > 0) {
                onLine(std::string_view(pendingLine.data(), separator));
            }
            std::size_t eraseCount = separator + 1U;
            while (eraseCount < pendingLine.size() &&
                   (pendingLine[eraseCount] == '\r' || pendingLine[eraseCount] == '\n')) {
                ++eraseCount;
            }
            pendingLine.erase(0, eraseCount);
        }
    }
    if (!pendingLine.empty()) {
        onLine(pendingLine);
    }

    WaitForSingleObject(processHandle.value, INFINITE);
    DWORD exitCode = ERROR_GEN_FAILURE;
    GetExitCodeProcess(processHandle.value, &exitCode);
    return {true, exitCode, {}};
}

[[nodiscard]] std::string Lowercase(std::string value) {
    std::transform(
        value.begin(), value.end(), value.begin(),
        [](const unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
    return value;
}

[[nodiscard]] std::uintmax_t SumFileSizes(const std::vector<fs::path>& files) {
    std::uintmax_t total = 0;
    for (const fs::path& path : files) {
        std::error_code error;
        const std::uintmax_t size = fs::file_size(path, error);
        if (!error) {
            total += size;
        }
    }
    return total;
}

[[nodiscard]] bool DetectLayout(
    const fs::path& directory,
    ExtractedLayout& layout,
    std::string& errorMessage) {
    std::vector<fs::path> audio;
    std::vector<fs::path> data;
    std::error_code error;
    for (const fs::directory_entry& entry : fs::directory_iterator(directory, error)) {
        if (error || !entry.is_regular_file()) {
            continue;
        }
        const std::string extension = Lowercase(entry.path().extension().string());
        if (extension == ".wav") {
            audio.push_back(entry.path());
        } else if (extension == ".iso") {
            data.push_back(entry.path());
        }
    }
    if (error) {
        errorMessage = "Could not inspect the extracted CDI tracks: " + error.message();
        return false;
    }
    std::sort(audio.begin(), audio.end());
    std::sort(data.begin(), data.end());

    if (!audio.empty() && !data.empty()) {
        layout.audioData = true;
        layout.firstSession = std::move(audio);
        layout.secondSession = std::move(data);
        layout.description = "Audio+Data self-boot CDI";
    } else if (audio.empty() && data.size() >= 2) {
        layout.audioData = false;
        layout.firstSession.push_back(data[0]);
        layout.secondSession.assign(data.begin() + 1, data.end());
        layout.description = "Data+Data self-boot CDI";
    } else {
        errorMessage =
            "CDIrip did not produce a supported two-session Dreamcast layout.";
        return false;
    }

    layout.firstSessionBytes = SumFileSizes(layout.firstSession);
    layout.secondSessionBytes = SumFileSizes(layout.secondSession);
    if (layout.firstSessionBytes == 0 || layout.secondSessionBytes == 0) {
        errorMessage = "One of the extracted CDI sessions is empty.";
        return false;
    }
    return true;
}

[[nodiscard]] const char* BurnTargetName(const BurnTarget target) {
    switch (target) {
    case BurnTarget::Dreamcast:
        return "Dreamcast";
    case BurnTarget::PlayStation:
        return "PlayStation";
    case BurnTarget::PlayStation2Cd:
        return "PlayStation 2 CD";
    case BurnTarget::PlayStation2Dvd:
        return "PlayStation 2 DVD";
    case BurnTarget::Saturn:
        return "Sega Saturn";
    case BurnTarget::Xbox360:
        return "Xbox 360";
    default:
        return "Unknown";
    }
}

[[nodiscard]] bool TargetUsesCue(const BurnTarget target) {
    return target == BurnTarget::PlayStation ||
           target == BurnTarget::Saturn;
}

[[nodiscard]] bool ReadCueInformation(
    const fs::path& cuePath,
    int& trackCount,
    std::string& errorMessage) {
    std::ifstream stream(cuePath, std::ios::binary);
    if (!stream) {
        errorMessage = "Could not open the selected CUE sheet.";
        return false;
    }

    static const std::regex trackPattern(
        "^\\s*TRACK\\s+[0-9]+\\s+\\S+",
        std::regex::icase);
    static const std::regex quotedFilePattern(
        "^\\s*FILE\\s+\"([^\"]+)\"\\s+\\S+",
        std::regex::icase);
    static const std::regex plainFilePattern(
        "^\\s*FILE\\s+([^\\s]+)\\s+\\S+",
        std::regex::icase);

    trackCount = 0;
    int fileCount = 0;
    std::string line;

    while (std::getline(stream, line)) {
        if (std::regex_search(line, trackPattern)) {
            ++trackCount;
        }

        std::smatch match;
        std::string referenced;
        if (std::regex_search(line, match, quotedFilePattern)) {
            referenced = match[1].str();
        } else if (std::regex_search(line, match, plainFilePattern)) {
            referenced = match[1].str();
        }

        if (!referenced.empty()) {
            ++fileCount;
            const fs::path referencedPath =
                cuePath.parent_path() / fs::path(referenced);
            std::error_code existsError;
            if (!fs::is_regular_file(referencedPath, existsError)) {
                errorMessage =
                    "CUE references a file that is missing: " +
                    referenced;
                return false;
            }
        }
    }

    if (fileCount == 0) {
        errorMessage = "The CUE sheet contains no FILE entry.";
        return false;
    }
    if (trackCount == 0) {
        errorMessage = "The CUE sheet contains no TRACK entries.";
        return false;
    }

    return true;
}

[[nodiscard]] bool ValidateStandardImage(
    const BurnRequest& request,
    std::string& layout,
    int& cueTrackCount,
    std::string& errorMessage) {
    const fs::path imagePath(request.cdiPath);
    std::error_code fileError;

    if (!fs::is_regular_file(imagePath, fileError)) {
        errorMessage = "Windows cannot find the selected disc image.";
        return false;
    }

    const std::string extension =
        Lowercase(imagePath.extension().string());

    if (request.target == BurnTarget::PlayStation ||
        request.target == BurnTarget::Saturn) {
        if (extension != ".cue") {
            errorMessage =
                std::string(BurnTargetName(request.target)) +
                " burning requires the .cue file so track layout and audio are preserved.";
            return false;
        }

        if (!ReadCueInformation(
                imagePath,
                cueTrackCount,
                errorMessage)) {
            return false;
        }

        layout =
            std::string(BurnTargetName(request.target)) +
            " BIN/CUE - " +
            std::to_string(cueTrackCount) +
            (cueTrackCount == 1 ? " track" : " tracks");
        return true;
    }

    if (request.target == BurnTarget::PlayStation2Cd) {
        if (extension == ".cue") {
            if (!ReadCueInformation(
                    imagePath,
                    cueTrackCount,
                    errorMessage)) {
                return false;
            }

            layout =
                "PlayStation 2 CD BIN/CUE - " +
                std::to_string(cueTrackCount) +
                (cueTrackCount == 1 ? " track" : " tracks");
            return true;
        }

        if (extension != ".iso") {
            errorMessage =
                "PlayStation 2 CD supports .cue (BIN/CUE) or .iso images.";
            return false;
        }

        const std::uintmax_t size = fs::file_size(imagePath, fileError);
        if (fileError || size == 0 || (size % 2048ULL) != 0ULL) {
            errorMessage =
                "The PS2 CD ISO is empty or is not aligned to 2048-byte sectors.";
            return false;
        }

        cueTrackCount = 1;
        layout = "PlayStation 2 CD ISO";
        return true;
    }

    if (request.target == BurnTarget::PlayStation2Dvd) {
        if (extension != ".iso") {
            errorMessage = "PlayStation 2 DVD supports .iso images.";
            return false;
        }

        const std::uintmax_t size = fs::file_size(imagePath, fileError);
        if (fileError || size == 0) {
            errorMessage = "Could not read the PS2 DVD ISO size.";
            return false;
        }
        if ((size % 2048ULL) != 0ULL) {
            errorMessage =
                "The PS2 DVD ISO is not aligned to 2048-byte DVD sectors.";
            return false;
        }

        constexpr std::uintmax_t kDvdRSingleLayerBytes = 4707319808ULL;
        constexpr std::uintmax_t kDvdPlusRDualLayerNominalBytes = 8547991552ULL;
        if (size > kDvdPlusRDualLayerNominalBytes) {
            errorMessage =
                "This PS2 ISO is larger than nominal dual-layer DVD capacity.";
            return false;
        }

        cueTrackCount = 1;
        layout = size > kDvdRSingleLayerBytes
            ? "PlayStation 2 DVD ISO - dual-layer DVD (growisofs)"
            : "PlayStation 2 DVD ISO - single-layer DVD (growisofs)";
        return true;
    }

    if (request.target == BurnTarget::Xbox360) {
        if (extension != ".iso") {
            errorMessage = "Xbox 360 burning currently supports .iso images.";
            return false;
        }

        const std::uintmax_t size = fs::file_size(imagePath, fileError);
        if (fileError || size == 0 || (size % 2048ULL) != 0ULL) {
            errorMessage =
                "The Xbox 360 ISO is empty or is not aligned to 2048-byte sectors.";
            return false;
        }

        constexpr std::uintmax_t kDvdPlusRDualLayerNominalBytes = 8547991552ULL;
        constexpr std::uintmax_t kXgd3FullImageBytes = 8738846720ULL;
        const std::uintmax_t maximum =
            request.xbox360DiscType == Xbox360DiscType::Xgd3
                ? kXgd3FullImageBytes
                : kDvdPlusRDualLayerNominalBytes;
        if (size > maximum) {
            errorMessage =
                request.xbox360DiscType == Xbox360DiscType::Xgd3
                    ? "This XGD3 ISO is larger than the supported full XGD3 image size."
                    : "This XGD2 ISO is larger than nominal DVD+R DL capacity.";
            return false;
        }

        cueTrackCount = 1;
        layout =
            request.xbox360DiscType == Xbox360DiscType::Xgd3
                ? "Xbox 360 XGD3 ISO - DVD+R DL / BurnerMAX"
                : "Xbox 360 XGD2 ISO - DVD+R DL";
        return true;
    }

    errorMessage = "Unsupported Retro Burner target.";
    return false;
}
[[nodiscard]] std::vector<std::uintmax_t> FileSizes(
    const std::vector<fs::path>& files) {
    std::vector<std::uintmax_t> sizes;
    sizes.reserve(files.size());
    for (const fs::path& path : files) {
        std::error_code error;
        sizes.push_back(fs::file_size(path, error));
        if (error) {
            sizes.back() = 0;
        }
    }
    return sizes;
}

[[nodiscard]] double UnitMultiplier(const std::string& unit) {
    if (unit == "KB" || unit == "kB") {
        return 1024.0;
    }
    if (unit == "MB") {
        return 1024.0 * 1024.0;
    }
    if (unit == "GB") {
        return 1024.0 * 1024.0 * 1024.0;
    }
    return 1.0;
}

[[nodiscard]] bool EjectOpticalDrive(const std::wstring& rootPath) {
    if (rootPath.size() < 2 || rootPath[1] != L':') {
        return false;
    }

    const std::wstring devicePath =
        L"\\\\.\\" + rootPath.substr(0, 2);

    UniqueHandle device(CreateFileW(
        devicePath.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr));
    if (!device.Valid()) {
        return false;
    }

    DWORD returned = 0;
    return DeviceIoControl(
               device.value,
               IOCTL_STORAGE_EJECT_MEDIA,
               nullptr,
               0,
               nullptr,
               0,
               &returned,
               nullptr) != FALSE;
}

void PlayBurnCompleteSound() {
    PlaySoundW(
        MAKEINTRESOURCEW(IDR_WAV_BURN_COMPLETE),
        GetModuleHandleW(nullptr),
        SND_RESOURCE | SND_ASYNC | SND_NODEFAULT);
}

} // namespace

bool BurnEngine::Start(BurnRequest request) {
    {
        std::lock_guard lock(mutex_);
        if (state_.busy) {
            return false;
        }
        state_ = BurnSnapshot{};
        state_.busy = true;
        state_.stage = BurnStage::Preparing;
        state_.status =
            request.checkOnly
                ? "Checking disc image..."
                : (request.simulate
                    ? "Preparing simulated write..."
                    : "Verifying burner and blank media...");
    }
    worker_ = std::jthread([this, request = std::move(request)]() mutable {
        Run(std::move(request));
    });
    return true;
}

BurnSnapshot BurnEngine::Snapshot() const {
    std::lock_guard lock(mutex_);
    return state_;
}

void BurnEngine::Reset() {
    std::lock_guard lock(mutex_);
    if (!state_.busy) {
        state_ = BurnSnapshot{};
    }
}

void BurnEngine::AppendLog(const std::string& text) {
    std::lock_guard lock(mutex_);
    if (text.empty()) {
        return;
    }

    state_.log += text;

    // cdrtools' Windows/Cygwin build prints a number of privilege warnings
    // even when SPTI access is working normally. They are expected noise in
    // Retro Burner and would otherwise look like fatal errors to users.
    static constexpr std::array<std::string_view, 11> benignCdrecordLines = {
        "cdrecord: Insufficient 'file read' privileges.",
        "cdrecord: Insufficient 'file write' privileges.",
        "cdrecord: Insufficient 'device' privileges.",
        "cdrecord: Insufficient 'memlock' privileges.",
        "cdrecord: Insufficient 'priocntl' privileges.",
        "cdrecord: Insufficient 'network' privileges.",
        "cdrecord: Warning: using inofficial version of libscg",
        "cdrecord: Warning: Cannot read drive buffer.",
        "cdrecord: Warning: The DMA speed test has been skipped.",
        "cdrecord: WARNING: Drive returns wrong startsec (0) using -150",
        "No Media Present or Unknown Capacity",
    };

    const auto removeCompleteLineContaining =
        [this](const std::string_view marker) {
            std::size_t searchFrom = 0;
            while (true) {
                const std::size_t hit =
                    state_.log.find(marker, searchFrom);
                if (hit == std::string::npos) {
                    break;
                }

                const std::size_t lineEnd =
                    state_.log.find('\n', hit);
                if (lineEnd == std::string::npos) {
                    // The child process may have split this line across pipe
                    // reads. Leave it until the next chunk completes it.
                    break;
                }

                const std::size_t previousNewline =
                    hit == 0
                        ? std::string::npos
                        : state_.log.rfind('\n', hit - 1);
                const std::size_t lineStart =
                    previousNewline == std::string::npos
                        ? 0
                        : previousNewline + 1;
                state_.log.erase(
                    lineStart,
                    lineEnd - lineStart + 1);
                searchFrom = lineStart;
            }
        };

    for (const std::string_view marker : benignCdrecordLines) {
        removeCompleteLineContaining(marker);
    }

    // cdrecord animates its ten-second countdown with terminal backspaces.
    // Replace that terminal-only line with one clean GUI-friendly message.
    constexpr std::string_view countdownMarker =
        "Last chance to quit, starting real write in";
    while (true) {
        const std::size_t hit =
            state_.log.find(countdownMarker);
        if (hit == std::string::npos) {
            break;
        }
        const std::size_t lineEnd =
            state_.log.find('\n', hit);
        if (lineEnd == std::string::npos) {
            break;
        }
        const std::size_t previousNewline =
            hit == 0
                ? std::string::npos
                : state_.log.rfind('\n', hit - 1);
        const std::size_t lineStart =
            previousNewline == std::string::npos
                ? 0
                : previousNewline + 1;
        state_.log.replace(
            lineStart,
            lineEnd - lineStart + 1,
            "Starting burn...\r\n");
    }

    state_.log.erase(
        std::remove(state_.log.begin(), state_.log.end(), '\b'),
        state_.log.end());

    if (state_.log.size() > kMaximumLogBytes) {
        state_.log.erase(
            0,
            state_.log.size() - kMaximumLogBytes);
    }
}
void BurnEngine::SetFailure(std::string message) {
    AppendLog("\r\nERROR: " + message + "\r\n");
    std::lock_guard lock(mutex_);
    state_.stage = BurnStage::Failed;
    state_.busy = false;
    state_.writing = false;
    state_.status = std::move(message);
}

void BurnEngine::RunStandardImage(
    BurnRequest request,
    const std::wstring& cdrecordPath,
    const std::wstring& growisofsPath,
    const std::wstring& dvdMediaInfoPath) {
    const fs::path cdrecord(cdrecordPath);
    const fs::path imagePath(request.cdiPath);
    const fs::path workingDirectory =
        imagePath.has_parent_path()
            ? imagePath.parent_path()
            : fs::current_path();

    std::string layout;
    std::string validationError;
    int cueTrackCount = 1;

    if (!ValidateStandardImage(
            request,
            layout,
            cueTrackCount,
            validationError)) {
        SetFailure(std::move(validationError));
        return;
    }

    {
        std::lock_guard lock(mutex_);
        state_.layout = layout;
        state_.status = "Image validated: " + layout;
    }

    AppendLog(
        "\r\nRetro Burner image check\r\n"
        "Target: " + std::string(BurnTargetName(request.target)) + "\r\n"
        "Image: " + imagePath.string() + "\r\n"
        "Layout: " + layout + "\r\n");

    if (request.checkOnly) {
        std::lock_guard lock(mutex_);
        state_.stage = BurnStage::Ready;
        state_.busy = false;
        state_.writing = false;
        state_.status =
            layout + ". Check complete; no disc was written.";
        return;
    }

    const bool dvdTarget =
        request.target == BurnTarget::PlayStation2Dvd ||
        request.target == BurnTarget::Xbox360;

    if (request.simulate && !dvdTarget) {
        SetFailure("Dry run is available only for DVD targets.");
        return;
    }

    if (dvdTarget) {
        if (request.opticalDriveRoot.empty()) {
            SetFailure("The selected Windows DVD drive has no drive letter.");
            return;
        }

        const fs::path growisofs(growisofsPath);
        const fs::path dvdMediaInfo(dvdMediaInfoPath);
        if (!fs::is_regular_file(growisofs) ||
            !fs::is_regular_file(dvdMediaInfo)) {
            SetFailure("The embedded DVD recording backend could not be prepared.");
            return;
        }

        std::string mediaInfoOutput;
        const auto appendMediaInfo =
            [this, &mediaInfoOutput](const std::string& text) {
                mediaInfoOutput += text;
                AppendLog(text);
            };
        const auto ignoreLine =
            [](std::string_view) {};

        AppendLog("\r\nDVD media preflight (dvd+rw-mediainfo)\r\n");
        const ProcessResult mediaInfo = RunHiddenProcess(
            dvdMediaInfo,
            {request.opticalDriveRoot},
            workingDirectory,
            appendMediaInfo,
            ignoreLine);

        if (!mediaInfo.started) {
            SetFailure(mediaInfo.error);
            return;
        }
        if (mediaInfo.exitCode != 0) {
            SetFailure(
                "dvd+rw-mediainfo could not validate the selected DVD writer/media. "
                "No image data was written.");
            return;
        }

        static const std::regex mountedProfilePattern(
            R"(Mounted Media:\s+([0-9A-Fa-f]+)h,)",
            std::regex::icase);
        static const std::regex blankDiscPattern(
            R"(Disc status:\s+blank)",
            std::regex::icase);
        static const std::regex freeBlocksPattern(
            R"(Free Blocks:\s+([0-9]+)\*2KB)",
            std::regex::icase);

        std::smatch mediaMatch;
        unsigned mediaProfile = 0;
        if (std::regex_search(
                mediaInfoOutput,
                mediaMatch,
                mountedProfilePattern)) {
            mediaProfile = static_cast<unsigned>(
                std::stoul(mediaMatch[1].str(), nullptr, 16));
        }

        if (!std::regex_search(mediaInfoOutput, blankDiscPattern)) {
            SetFailure(
                "The selected DVD media is not positively reported as blank. "
                "No image data was written.");
            return;
        }

        std::error_code sizeError;
        const std::uintmax_t imageBytes =
            fs::file_size(imagePath, sizeError);
        if (sizeError || imageBytes == 0) {
            SetFailure("Could not read the selected image size.");
            return;
        }

        constexpr std::uintmax_t kDvdSingleLayerBytes = 4707319808ULL;
        const bool ps2NeedsDualLayer =
            request.target == BurnTarget::PlayStation2Dvd &&
            imageBytes > kDvdSingleLayerBytes;

        const bool isSingleLayerRecordable =
            mediaProfile == 0x11 || // DVD-R
            mediaProfile == 0x1B;   // DVD+R
        const bool isDualLayerRecordable =
            mediaProfile == 0x15 || // DVD-R DL sequential
            mediaProfile == 0x16 || // DVD-R DL layer jump
            mediaProfile == 0x2B;   // DVD+R DL

        if (request.target == BurnTarget::Xbox360) {
            if (mediaProfile != 0x2B) {
                SetFailure(
                    "Xbox 360 backups require blank DVD+R DL media. "
                    "DVD-R DL and single-layer media are not accepted.");
                return;
            }
        } else if (ps2NeedsDualLayer) {
            if (!isDualLayerRecordable) {
                SetFailure(
                    "This PS2 ISO requires dual-layer media. Insert a blank "
                    "DVD-R DL or DVD+R DL and retry.");
                return;
            }
        } else if (!isSingleLayerRecordable && !isDualLayerRecordable) {
            SetFailure(
                "PlayStation 2 DVD burning requires blank DVD-R, DVD+R, "
                "DVD-R DL or DVD+R DL media.");
            return;
        }

        if (std::regex_search(
                mediaInfoOutput,
                mediaMatch,
                freeBlocksPattern)) {
            const std::uintmax_t freeBlocks =
                std::stoull(mediaMatch[1].str());
            const std::uintmax_t reportedCapacity =
                freeBlocks * 2048ULL;
            if (imageBytes > reportedCapacity) {
                if (request.target == BurnTarget::Xbox360 &&
                    request.xbox360DiscType == Xbox360DiscType::Xgd3) {
                    SetFailure(
                        "The XGD3 image is larger than the capacity reported by this "
                        "drive/media combination. XGD3 requires a BurnerMAX-compatible "
                        "DVD+R DL writer/firmware with expanded writable capacity.");
                } else {
                    SetFailure(
                        "The selected image does not fit the writable capacity reported "
                        "for the inserted DVD.");
                }
                return;
            }
        }

        const bool xbox = request.target == BurnTarget::Xbox360;
        const bool xgd3 =
            xbox &&
            request.xbox360DiscType == Xbox360DiscType::Xgd3;
        const std::string targetDescription =
            xbox
                ? (xgd3 ? "Xbox 360 XGD3" : "Xbox 360 XGD2")
                : "PlayStation 2 DVD";

        {
            std::lock_guard lock(mutex_);
            state_.stage = BurnStage::BurningSession1;
            state_.busy = true;
            state_.writing = !request.simulate;
            state_.session = 1;
            state_.progress = 0.0F;
            state_.bufferPercent = -1;
            state_.ringBufferPercent = -1;
            state_.driveBufferPercent = -1;
            state_.actualSpeed.clear();
            state_.remainingTime.clear();
            state_.status = request.simulate
                ? targetDescription + " dry run - no write..."
                : "Writing " + targetDescription + "...";
        }

        std::vector<std::wstring> arguments;
        if (request.simulate) {
            arguments.emplace_back(L"-dry-run");
        }
        if (xbox) {
            arguments.emplace_back(L"-use-the-force-luke=dao");
            arguments.push_back(
                L"-use-the-force-luke=break:" +
                std::to_wstring(xgd3 ? 2133520 : 1913760));
        }
        arguments.emplace_back(L"-dvd-compat");
        if (request.requestedSpeedX > 0) {
            arguments.push_back(
                L"-speed=" + std::to_wstring(request.requestedSpeedX));
        }
        arguments.emplace_back(L"-Z");
        arguments.push_back(
            request.opticalDriveRoot + L"=" + imagePath.wstring());

        AppendLog(
            request.simulate
                ? "\r\nDVD DRY RUN - growisofs -dry-run\r\n"
                : "\r\nDVD WRITE - growisofs\r\n");

        static const std::regex growProgressPattern(
            R"(\(\s*([0-9]+(?:\.[0-9]+)?)%\)\s+@([0-9]+(?:\.[0-9]+)?)x,\s+remaining\s+([0-9?:]+)\s+RBU\s+([0-9]+(?:\.[0-9]+)?)%\s+UBU\s+([0-9]+(?:\.[0-9]+)?)%)",
            std::regex::icase);

        const OutputCallback parseGrowisofs =
            [this, targetDescription](const std::string_view lineView) {
                const std::string line(lineView);
                std::smatch match;
                if (!std::regex_search(
                        line,
                        match,
                        growProgressPattern)) {
                    return;
                }

                const float percent =
                    std::stof(match[1].str());
                const std::string speed =
                    match[2].str() + "x";
                const std::string remaining =
                    match[3].str();
                const int rbu =
                    static_cast<int>(std::lround(
                        std::stod(match[4].str())));
                const int ubu =
                    static_cast<int>(std::lround(
                        std::stod(match[5].str())));

                std::lock_guard lock(mutex_);
                state_.progress = std::clamp(
                    percent / 100.0F,
                    state_.progress,
                    0.999F);
                state_.actualSpeed = speed;
                state_.remainingTime = remaining;
                state_.ringBufferPercent =
                    std::clamp(rbu, 0, 100);
                state_.driveBufferPercent =
                    std::clamp(ubu, 0, 100);
                state_.status =
                    "Writing " + targetDescription + " - " +
                    std::to_string(
                        static_cast<int>(std::lround(percent))) +
                    "%";
            };

        const auto append =
            [this](const std::string& text) {
                AppendLog(text);
            };

        const ProcessResult result = RunHiddenProcess(
            growisofs,
            arguments,
            workingDirectory,
            append,
            parseGrowisofs);

        if (!result.started) {
            SetFailure(result.error);
            return;
        }
        if (result.exitCode != 0) {
            SetFailure(
                request.simulate
                    ? targetDescription +
                          " dry run failed. No image data was intentionally written."
                    : targetDescription +
                          " write failed. The disc may be incomplete; check the burn log.");
            return;
        }

        if (request.simulate) {
            std::lock_guard lock(mutex_);
            state_.stage = BurnStage::Ready;
            state_.busy = false;
            state_.writing = false;
            state_.progress = 0.0F;
            state_.status =
                targetDescription +
                " dry run completed successfully. No image data was written.";
            return;
        }

        const bool ejected =
            EjectOpticalDrive(request.opticalDriveRoot);
        if (!ejected) {
            AppendLog(
                "\r\nWARNING: Burn succeeded, but Windows could not automatically eject the disc.\r\n");
        }

        PlayBurnCompleteSound();

        std::lock_guard lock(mutex_);
        state_.stage = BurnStage::Complete;
        state_.busy = false;
        state_.writing = false;
        state_.progress = 1.0F;
        state_.session = 1;
        state_.remainingTime = "00:00";
        state_.status = ejected
            ? "Burn complete! Disc ejected."
            : "Burn complete!";
        return;
    }

    const auto append =
        [this](const std::string& text) {
            AppendLog(text);
        };
    const auto ignoreLine =
        [](std::string_view) {};

    AppendLog("\r\nRead-only media preflight\r\n");
    const std::vector<std::wstring> preflightArguments = {
        L"dev=" + std::wstring(
            request.cdrecordDevice.begin(),
            request.cdrecordDevice.end()),
        L"-atip",
    };

    const ProcessResult preflight = RunHiddenProcess(
        cdrecord,
        preflightArguments,
        workingDirectory,
        append,
        ignoreLine);

    if (!preflight.started) {
        SetFailure(preflight.error);
        return;
    }
    if (preflight.exitCode != 0) {
        SetFailure(
            "The selected burner or inserted blank media failed preflight. "
            "No image data was written.");
        return;
    }

    {
        std::lock_guard lock(mutex_);
        state_.stage = BurnStage::BurningSession1;
        state_.busy = true;
        state_.writing = true;
        state_.session = 1;
        state_.bufferPercent = -1;
        state_.ringBufferPercent = -1;
        state_.driveBufferPercent = -1;
        state_.remainingTime.clear();
        state_.actualSpeed.clear();
        state_.progress = 0.0F;
        state_.status =
            "Writing " + std::string(BurnTargetName(request.target)) + " disc...";
    }

    AppendLog("\r\nWriting disc\r\n");

    std::vector<std::wstring> arguments;
    arguments.push_back(
        L"dev=" + std::wstring(
            request.cdrecordDevice.begin(),
            request.cdrecordDevice.end()));
    arguments.emplace_back(L"-v");

    if (request.requestedSpeedX > 0) {
        arguments.push_back(
            L"-speed=" +
            std::to_wstring(request.requestedSpeedX));
    }

    arguments.emplace_back(L"-eject");
    arguments.emplace_back(L"-dao");

    const std::string extension =
        Lowercase(imagePath.extension().string());

    if (extension == ".cue") {
        arguments.push_back(
            L"cuefile=" + imagePath.wstring());
    } else {
        arguments.emplace_back(L"-data");
        arguments.push_back(imagePath.wstring());
    }

    static const std::regex progressPattern(
        R"(Track\s+(\d+):\s+([0-9]+(?:\.[0-9]+)?)\s+of\s+([0-9]+(?:\.[0-9]+)?)\s+([kMGT]?B)\s+written)",
        std::regex::icase);
    static const std::regex bufferPattern(
        R"(\[buf\s*([0-9]+)%\])");
    static const std::regex speedPattern(
        R"(([0-9]+(?:\.[0-9]+)?)x)");
    static const std::regex startSpeedPattern(
        R"(Starting to write.*speed\s+([0-9]+(?:\.[0-9]+)?))",
        std::regex::icase);

    const int totalTracks = std::max(1, cueTrackCount);
    const std::string targetName = BurnTargetName(request.target);

    const OutputCallback parseLine =
        [this, totalTracks, targetName](const std::string_view lineView) {
            const std::string line(lineView);
            std::smatch match;
            float progress = -1.0F;
            int bufferPercent = -1;
            std::string speed;

            if (std::regex_search(
                    line,
                    match,
                    progressPattern)) {
                const int trackNumber =
                    std::clamp(
                        std::stoi(match[1].str()),
                        1,
                        totalTracks);
                const double written =
                    std::stod(match[2].str()) *
                    UnitMultiplier(match[4].str());
                const double total =
                    std::stod(match[3].str()) *
                    UnitMultiplier(match[4].str());
                const double trackFraction =
                    total > 0.0
                        ? std::clamp(
                            written / total,
                            0.0,
                            1.0)
                        : 0.0;

                progress = static_cast<float>(
                    std::clamp(
                        (static_cast<double>(trackNumber - 1) +
                         trackFraction) /
                            static_cast<double>(totalTracks),
                        0.0,
                        0.999));
            }

            if (std::regex_search(
                    line,
                    match,
                    bufferPattern)) {
                bufferPercent =
                    std::stoi(match[1].str());
            }

            if (std::regex_search(
                    line,
                    match,
                    startSpeedPattern)) {
                speed = match[1].str() + "x";
            } else if (std::regex_search(
                           line,
                           match,
                           speedPattern)) {
                speed = match[1].str() + "x";
            }

            if (progress >= 0.0F ||
                bufferPercent >= 0 ||
                !speed.empty()) {
                std::lock_guard lock(mutex_);

                if (progress >= 0.0F) {
                    state_.progress =
                        std::max(
                            state_.progress,
                            progress);
                    state_.status =
                        "Writing " + targetName + " - " +
                        std::to_string(
                            static_cast<int>(std::lround(
                                state_.progress * 100.0F))) +
                        "%";
                }
                if (bufferPercent >= 0) {
                    state_.bufferPercent =
                        std::clamp(bufferPercent, 0, 100);
                }
                if (!speed.empty()) {
                    state_.actualSpeed =
                        std::move(speed);
                }
            }
        };

    const ProcessResult result = RunHiddenProcess(
        cdrecord,
        arguments,
        workingDirectory,
        append,
        parseLine);

    if (!result.started) {
        SetFailure(result.error);
        return;
    }

    if (result.exitCode != 0) {
        SetFailure(
            std::string(BurnTargetName(request.target)) +
            " write failed. The disc may be incomplete; check the burn log.");
        return;
    }

    PlayBurnCompleteSound();

    std::lock_guard lock(mutex_);
    state_.busy = false;
    state_.writing = false;
    state_.stage = BurnStage::Complete;
    state_.progress = 1.0F;
    state_.session = 1;
    state_.status = "Burn complete! Disc ejected.";
}
void BurnEngine::Run(BurnRequest request) {
    const EmbeddedToolPaths& tools = GetEmbeddedToolPaths();
    if (!tools.Ready()) {
        SetFailure("The embedded recording backend could not be prepared: " + tools.error);
        return;
    }

    const fs::path appDirectory = tools.directory;
    const fs::path cdirip = tools.cdirip;
    const fs::path cdrecord = tools.cdrecord;
    const fs::path growisofs = tools.growisofs;
    const fs::path dvdMediaInfo = tools.dvdMediaInfo;

    if (request.target != BurnTarget::Dreamcast) {
        RunStandardImage(
            std::move(request),
            cdrecord.wstring(),
            growisofs.wstring(),
            dvdMediaInfo.wstring());
        return;
    }
    TemporaryDirectory temporary = MakeTemporaryDirectory();
    if (temporary.path.empty()) {
        SetFailure("Windows could not create a temporary extraction directory.");
        return;
    }

    std::error_code fileError;
    const std::uintmax_t cdiSize = fs::file_size(request.cdiPath, fileError);
    ULARGE_INTEGER freeBytes{};
    if (!fileError && GetDiskFreeSpaceExW(
            temporary.path.c_str(), &freeBytes, nullptr, nullptr) &&
        freeBytes.QuadPart < cdiSize + kMinimumFreeSpaceMargin) {
        SetFailure("Not enough free temporary disk space to extract this CDI.");
        return;
    }

    const auto append = [this](const std::string& text) { AppendLog(text); };
    const auto ignoreLine = [](std::string_view) {};

    if (!request.checkOnly) {
        AppendLog("Dreamcast Burner - read-only media preflight\r\n");
        const std::vector<std::wstring> preflightArguments = {
            L"dev=" + std::wstring(
                request.cdrecordDevice.begin(), request.cdrecordDevice.end()),
            L"-atip",
        };
        const ProcessResult preflight = RunHiddenProcess(
            cdrecord,
            preflightArguments,
            appDirectory,
            append,
            ignoreLine);
        if (!preflight.started) {
            SetFailure(preflight.error);
            return;
        }
        if (preflight.exitCode != 0) {
            SetFailure(
                "The selected burner or blank CD-R failed the read-only preflight. "
                "No data was written; open Burn log for cdrecord's reason.");
            return;
        }
    }

    {
        std::lock_guard lock(mutex_);
        state_.status = "Extracting and analysing CDI tracks...";
    }
    AppendLog("\r\nCDIrip extraction\r\n");
    const std::vector<std::wstring> extractArguments = {
        request.cdiPath,
        temporary.path.wstring(),
        // Match DCDIB's all-layout extraction behaviour on Windows:
        // force data tracks to ISO, but do NOT use CDIrip's -cdrecord
        // preset because that also enables -cutall and trims audio tracks.
        L"-iso",
    };
    const ProcessResult extraction = RunHiddenProcess(
        cdirip,
        extractArguments,
        temporary.path,
        append,
        ignoreLine);
    if (!extraction.started) {
        SetFailure(extraction.error);
        return;
    }
    if (extraction.exitCode != 0) {
        SetFailure("CDIrip could not extract this image. Open Burn log for details.");
        return;
    }

    ExtractedLayout layout;
    std::string layoutError;
    if (!DetectLayout(temporary.path, layout, layoutError)) {
        SetFailure(std::move(layoutError));
        return;
    }

    {
        std::lock_guard lock(mutex_);
        state_.layout = layout.description;
    }
    AppendLog("\r\nDetected: " + layout.description + "\r\n");
    if (request.checkOnly) {
        std::lock_guard lock(mutex_);
        state_.stage = BurnStage::Ready;
        state_.busy = false;
        state_.status = layout.description + ". Check complete; no disc was written.";
        return;
    }

    const std::uintmax_t totalBytes =
        layout.firstSessionBytes + layout.secondSessionBytes;

    const auto runSession = [this, &cdrecord, &temporary, &request, totalBytes](
                                const int sessionNumber,
                                const bool audioSession,
                                const std::vector<fs::path>& tracks,
                                const std::uintmax_t priorSessionBytes) -> bool {
        const std::vector<std::uintmax_t> trackSizes = FileSizes(tracks);
        std::vector<std::uintmax_t> priorTrackBytes(trackSizes.size(), 0);
        for (std::size_t index = 1; index < trackSizes.size(); ++index) {
            priorTrackBytes[index] = priorTrackBytes[index - 1] + trackSizes[index - 1];
        }

        {
            std::lock_guard lock(mutex_);
            state_.stage = sessionNumber == 1
                ? BurnStage::BurningSession1
                : BurnStage::BurningSession2;
            state_.busy = true;
            state_.writing = true;
            state_.session = sessionNumber;
            state_.bufferPercent = -1;
            state_.actualSpeed.clear();
            state_.status = "Burning session " + std::to_string(sessionNumber) + " of 2";
        }

        AppendLog("\r\nBurning session " + std::to_string(sessionNumber) + " of 2\r\n");
        std::vector<std::wstring> arguments;
        arguments.push_back(
            L"dev=" + std::wstring(
                request.cdrecordDevice.begin(), request.cdrecordDevice.end()));
        arguments.emplace_back(L"-v");
        if (request.requestedSpeedX > 0) {
            arguments.push_back(L"-speed=" + std::to_wstring(request.requestedSpeedX));
        }
        if (sessionNumber == 1) {
            arguments.emplace_back(audioSession ? L"-dao" : L"-tao");
            arguments.emplace_back(L"-multi");
            if (!audioSession) {
                arguments.emplace_back(L"-xa");
            }
        } else {
            arguments.emplace_back(L"-eject");
            arguments.emplace_back(L"-overburn");
            arguments.emplace_back(L"-tao");
            arguments.emplace_back(L"-xa");
        }
        for (const fs::path& track : tracks) {
            arguments.push_back(track.filename().wstring());
        }

        static const std::regex progressPattern(
            R"(Track\s+(\d+):\s+([0-9]+(?:\.[0-9]+)?)\s+of\s+([0-9]+(?:\.[0-9]+)?)\s+([kMGT]?B)\s+written)",
            std::regex::icase);
        static const std::regex bufferPattern(R"(\[buf\s*([0-9]+)%\])");
        static const std::regex speedPattern(R"(([0-9]+(?:\.[0-9]+)?)x)");
        static const std::regex startSpeedPattern(
            R"(Starting to write.*speed\s+([0-9]+(?:\.[0-9]+)?))",
            std::regex::icase);

        const OutputCallback parseLine =
            [this, &trackSizes, &priorTrackBytes, priorSessionBytes, totalBytes](
                const std::string_view lineView) {
                const std::string line(lineView);
                std::smatch match;
                float overallProgress = -1.0F;
                int bufferPercent = -1;
                std::string speed;

                if (std::regex_search(line, match, progressPattern)) {
                    const int trackNumber = std::max(1, std::stoi(match[1].str()));
                    const std::size_t trackIndex = std::min<std::size_t>(
                        static_cast<std::size_t>(trackNumber - 1),
                        trackSizes.empty() ? 0U : trackSizes.size() - 1U);
                    const double written = std::stod(match[2].str()) *
                        UnitMultiplier(match[4].str());
                    const double reportedTotal = std::stod(match[3].str()) *
                        UnitMultiplier(match[4].str());
                    const double fraction = reportedTotal > 0.0
                        ? std::clamp(written / reportedTotal, 0.0, 1.0)
                        : 0.0;
                    const double completed = static_cast<double>(priorSessionBytes) +
                        static_cast<double>(priorTrackBytes[trackIndex]) +
                        fraction * static_cast<double>(trackSizes[trackIndex]);
                    overallProgress = static_cast<float>(std::clamp(
                        completed / static_cast<double>(totalBytes), 0.0, 0.999));
                }
                if (std::regex_search(line, match, bufferPattern)) {
                    bufferPercent = std::stoi(match[1].str());
                }
                if (std::regex_search(line, match, startSpeedPattern)) {
                    speed = match[1].str() + "x";
                } else if (std::regex_search(line, match, speedPattern)) {
                    speed = match[1].str() + "x";
                }

                if (overallProgress >= 0.0F || bufferPercent >= 0 || !speed.empty()) {
                    std::lock_guard lock(mutex_);
                    if (overallProgress >= 0.0F) {
                        state_.progress = std::max(state_.progress, overallProgress);
                    }
                    if (bufferPercent >= 0) {
                        state_.bufferPercent = bufferPercent;
                    }
                    if (!speed.empty()) {
                        state_.actualSpeed = std::move(speed);
                    }
                }
            };

        const ProcessResult result = RunHiddenProcess(
            cdrecord,
            arguments,
            temporary.path,
            [this](const std::string& text) { AppendLog(text); },
            parseLine);
        if (!result.started) {
            SetFailure(result.error);
            return false;
        }
        if (result.exitCode != 0) {
            SetFailure(
                "Session " + std::to_string(sessionNumber) +
                " failed. The disc is incomplete and cannot be reused; open Burn log for details.");
            return false;
        }
        return true;
    };

    if (!runSession(1, layout.audioData, layout.firstSession, 0)) {
        return;
    }
    if (!runSession(
            2,
            false,
            layout.secondSession,
            layout.firstSessionBytes)) {
        return;
    }

    AppendLog("\r\nBurn complete.\r\n");
    PlayBurnCompleteSound();

    std::lock_guard lock(mutex_);
    state_.stage = BurnStage::Complete;
    state_.busy = false;
    state_.writing = false;
    state_.progress = 1.0F;
    state_.session = 2;
    state_.status = "Burn complete! Disc ejected.";
}
