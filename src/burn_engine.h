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

struct BurnRequest final {
    std::wstring cdiPath;
    std::string cdrecordDevice;
    int requestedSpeedX = 0; // 0 lets the drive choose.
    bool checkOnly = false;
};

struct BurnSnapshot final {
    BurnStage stage = BurnStage::Idle;
    bool busy = false;
    bool writing = false;
    float progress = 0.0F;
    int session = 0;
    int bufferPercent = -1;
    std::string actualSpeed;
    std::string layout;
    std::string status = "Choose a CDI image and insert a blank CD-R.";
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
    void SetFailure(std::string message);
    void AppendLog(const std::string& text);

    mutable std::mutex mutex_;
    BurnSnapshot state_;
    std::jthread worker_;
};
