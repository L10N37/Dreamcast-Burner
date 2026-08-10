#pragma once

#include <mutex>
#include <string>
#include <thread>

enum class BurnStage {
    Idle,
    Preparing,
    Ready,
    BurningSession1,
    BurningSession2,
    Complete,
    Failed,
};

enum class BurnTarget {
    Dreamcast,
    PlayStation,
    PlayStation2Cd,
    PlayStation2Dvd,
    Saturn,
};

struct BurnRequest final {
    // Kept as cdiPath for compatibility with the existing UI/state while
    // Retro Burner grows beyond CDI. It now contains the selected image path.
    std::wstring cdiPath;
    BurnTarget target = BurnTarget::Dreamcast;
    std::string cdrecordDevice;
    std::wstring opticalDriveRoot; // e.g. L"K:" for native Windows DVD tools.
    int requestedSpeedX = 0; // 0 lets the drive/media choose.
    bool checkOnly = false;
    bool simulate = false;   // cdrecord -dummy: laser off when supported.
};

struct BurnSnapshot final {
    BurnStage stage = BurnStage::Idle;
    bool busy = false;
    bool writing = false;
    float progress = 0.0F;
    int session = 0;
    int bufferPercent = -1;
    int ringBufferPercent = -1;
    int driveBufferPercent = -1;
    std::string actualSpeed;
    std::string remainingTime;
    std::string layout;
    std::string status = "Choose a disc image.";
    std::string log;
};

class BurnEngine final {
public:
    BurnEngine() = default;
    ~BurnEngine() = default;

    BurnEngine(const BurnEngine&) = delete;
    BurnEngine& operator=(const BurnEngine&) = delete;

    [[nodiscard]] bool Start(BurnRequest request);
    [[nodiscard]] BurnSnapshot Snapshot() const;
    void Reset();

private:
    void Run(BurnRequest request);
    void RunStandardImage(
        BurnRequest request,
        const std::wstring& cdrecordPath,
        const std::wstring& growisofsPath,
        const std::wstring& dvdMediaInfoPath);
    void SetFailure(std::string message);
    void AppendLog(const std::string& text);

    mutable std::mutex mutex_;
    BurnSnapshot state_;
    std::jthread worker_;
};
