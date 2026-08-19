#include <windows.h>
#include <winioctl.h>
#include <ntddscsi.h>
#include <ntddstor.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

/*
 * RB_BURNERMAX_READONLY_LAB_35
 *
 * Read-only BurnerMAX laboratory helper.
 *
 * Deliberately absent:
 *   F1/01 register write
 *   DF/84 register write
 *   WRITE BUFFER
 *   MODE SELECT
 *   firmware update / bootloader commands
 *   media writes
 */

namespace {

constexpr std::uint16_t kRegisterBase = 0x8000;
constexpr std::uint16_t kRegisterEnd = 0x9000;
constexpr std::size_t kRegisterCount =
    static_cast<std::size_t>(kRegisterEnd - kRegisterBase);
constexpr ULONG kTimeoutSeconds = 15;

struct UniqueHandle {
    HANDLE value = INVALID_HANDLE_VALUE;
    ~UniqueHandle() {
        if (value != INVALID_HANDLE_VALUE) {
            CloseHandle(value);
        }
    }
};

struct ScsiPacket {
    SCSI_PASS_THROUGH_DIRECT sptd{};
    ULONG filler = 0;
    std::array<UCHAR, 64> sense{};
};

struct ScsiResult {
    bool ioctlOk = false;
    DWORD winError = 0;
    UCHAR scsiStatus = 0;
    unsigned senseKey = 0;
    unsigned asc = 0;
    unsigned ascq = 0;
    long long elapsedMs = 0;

    bool Good() const {
        return ioctlOk && scsiStatus == 0;
    }
};

std::string Hex(unsigned value, unsigned width) {
    std::ostringstream ss;
    ss << std::uppercase << std::hex << std::setfill('0')
       << std::setw(static_cast<int>(width)) << value;
    return ss.str();
}

std::wstring DevicePath(const std::wstring& driveArg) {
    if (driveArg.empty()) {
        return {};
    }

    wchar_t letter = driveArg[0];
    if (letter >= L'a' && letter <= L'z') {
        letter = static_cast<wchar_t>(letter - L'a' + L'A');
    }

    if (letter < L'A' || letter > L'Z') {
        return {};
    }

    std::wstring result = L"\\\\.\\";
    result.push_back(letter);
    result.push_back(L':');
    return result;
}

ULONG QueryAlignmentMask(HANDLE drive) {
    STORAGE_PROPERTY_QUERY query{};
    query.PropertyId = StorageAdapterProperty;
    query.QueryType = PropertyStandardQuery;

    alignas(64) std::array<UCHAR, 512> buffer{};
    DWORD returned = 0;

    if (DeviceIoControl(
            drive,
            IOCTL_STORAGE_QUERY_PROPERTY,
            &query,
            sizeof(query),
            buffer.data(),
            static_cast<DWORD>(buffer.size()),
            &returned,
            nullptr) &&
        returned >= offsetof(STORAGE_ADAPTER_DESCRIPTOR, AlignmentMask) +
                        sizeof(ULONG)) {
        const auto* descriptor =
            reinterpret_cast<const STORAGE_ADAPTER_DESCRIPTOR*>(buffer.data());
        return descriptor->AlignmentMask;
    }

    return 4095;
}

ScsiResult SendScsi(
    HANDLE drive,
    ULONG alignmentMask,
    const std::array<UCHAR, 12>& cdb,
    UCHAR direction,
    void* data,
    ULONG dataLength) {

    ScsiPacket packet{};
    packet.sptd.Length =
        static_cast<USHORT>(sizeof(SCSI_PASS_THROUGH_DIRECT));
    packet.sptd.CdbLength = 12;
    packet.sptd.SenseInfoLength =
        static_cast<UCHAR>(packet.sense.size());
    packet.sptd.DataIn = direction;
    packet.sptd.DataTransferLength = dataLength;
    packet.sptd.TimeOutValue = kTimeoutSeconds;
    packet.sptd.SenseInfoOffset =
        static_cast<ULONG>(offsetof(ScsiPacket, sense));
    std::copy(cdb.begin(), cdb.end(), packet.sptd.Cdb);

    std::vector<UCHAR> storage;
    UCHAR* aligned = nullptr;

    if (dataLength != 0) {
        storage.resize(
            static_cast<std::size_t>(dataLength) +
            static_cast<std::size_t>(alignmentMask));

        const std::uintptr_t raw =
            reinterpret_cast<std::uintptr_t>(storage.data());
        const std::uintptr_t alignedAddress =
            (raw + alignmentMask) & ~static_cast<std::uintptr_t>(alignmentMask);
        aligned = reinterpret_cast<UCHAR*>(alignedAddress);

        if (direction == SCSI_IOCTL_DATA_OUT && data != nullptr) {
            std::memcpy(aligned, data, dataLength);
        }
    }

    packet.sptd.DataBuffer = aligned;

    ScsiResult result{};
    DWORD returned = 0;

    const auto begin = std::chrono::steady_clock::now();

    result.ioctlOk =
        DeviceIoControl(
            drive,
            IOCTL_SCSI_PASS_THROUGH_DIRECT,
            &packet,
            sizeof(packet),
            &packet,
            sizeof(packet),
            &returned,
            nullptr) != FALSE;

    const auto end = std::chrono::steady_clock::now();

    result.elapsedMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            end - begin).count();

    if (!result.ioctlOk) {
        result.winError = GetLastError();
        return result;
    }

    result.scsiStatus = packet.sptd.ScsiStatus;
    result.senseKey =
        packet.sense.size() > 2 ? packet.sense[2] & 0x0F : 0;
    result.asc =
        packet.sense.size() > 12 ? packet.sense[12] : 0;
    result.ascq =
        packet.sense.size() > 13 ? packet.sense[13] : 0;

    if (result.Good() &&
        direction == SCSI_IOCTL_DATA_IN &&
        data != nullptr &&
        aligned != nullptr &&
        dataLength != 0) {
        const ULONG transferred =
            std::min(dataLength, packet.sptd.DataTransferLength);
        std::memcpy(data, aligned, transferred);
    }

    return result;
}

void PrintResult(
    const std::string& name,
    const std::array<UCHAR, 12>& cdb,
    const ScsiResult& r) {

    std::cout
        << "RBMAXPROBE CMD name=" << name
        << " op=0x" << Hex(cdb[0], 2)
        << " sub=0x" << Hex(cdb[1], 2)
        << " ioctl=" << (r.ioctlOk ? 1 : 0)
        << " winerr=" << r.winError
        << " scsi=0x" << Hex(r.scsiStatus, 2)
        << " sense=" << Hex(r.senseKey, 2)
        << "/" << Hex(r.asc, 2)
        << "/" << Hex(r.ascq, 2)
        << " elapsed_ms=" << r.elapsedMs
        << "\n";
}

bool ReadInquiry(
    HANDLE drive,
    ULONG alignmentMask) {

    alignas(64) std::array<UCHAR, 96> data{};
    std::array<UCHAR, 12> cdb{};
    cdb[0] = 0x12;
    cdb[4] = static_cast<UCHAR>(data.size());

    const ScsiResult r =
        SendScsi(
            drive,
            alignmentMask,
            cdb,
            SCSI_IOCTL_DATA_IN,
            data.data(),
            static_cast<ULONG>(data.size()));

    PrintResult("INQUIRY", cdb, r);

    if (!r.Good()) {
        return false;
    }

    std::string vendor(
        reinterpret_cast<char*>(&data[8]),
        reinterpret_cast<char*>(&data[16]));
    std::string product(
        reinterpret_cast<char*>(&data[16]),
        reinterpret_cast<char*>(&data[32]));
    std::string revision(
        reinterpret_cast<char*>(&data[32]),
        reinterpret_cast<char*>(&data[36]));

    std::cout
        << "RBMAXPROBE ID vendor='" << vendor
        << "' product='" << product
        << "' revision='" << revision
        << "'\n";

    return true;
}

bool ReadLayerBoundary(
    HANDLE drive,
    ULONG alignmentMask,
    std::uint32_t& boundary) {

    alignas(64) std::array<UCHAR, 12> data{};
    std::array<UCHAR, 12> cdb{};
    cdb[0] = 0xAD;
    cdb[7] = 0x20;
    cdb[9] = static_cast<UCHAR>(data.size());

    const ScsiResult r =
        SendScsi(
            drive,
            alignmentMask,
            cdb,
            SCSI_IOCTL_DATA_IN,
            data.data(),
            static_cast<ULONG>(data.size()));

    PrintResult("READ_DVD_STRUCTURE_20", cdb, r);

    if (!r.Good()) {
        return false;
    }

    boundary =
        (static_cast<std::uint32_t>(data[9]) << 16U) |
        (static_cast<std::uint32_t>(data[10]) << 8U) |
        static_cast<std::uint32_t>(data[11]);

    std::cout
        << "RBMAXPROBE LAYER_BOUNDARY value="
        << boundary
        << " hex=0x" << Hex(boundary, 6)
        << "\n";

    return true;
}

bool ReadF1(
    HANDLE drive,
    ULONG alignmentMask,
    std::array<std::uint8_t, kRegisterCount>& dump) {

    for (std::uint32_t address = kRegisterBase;
         address < kRegisterEnd;
         ++address) {

        alignas(64) std::array<UCHAR, 4> data{};
        std::array<UCHAR, 12> cdb{};
        cdb[0] = 0xF1;
        cdb[1] = 0x02;
        cdb[4] = static_cast<UCHAR>(address >> 8U);
        cdb[5] = static_cast<UCHAR>(address & 0xFFU);
        cdb[6] = 0x01;

        const ScsiResult r =
            SendScsi(
                drive,
                alignmentMask,
                cdb,
                SCSI_IOCTL_DATA_IN,
                data.data(),
                static_cast<ULONG>(data.size()));

        if (!r.Good()) {
            std::cout
                << "RBMAXPROBE F1 result=unavailable address=0x"
                << Hex(address, 4)
                << " ioctl=" << (r.ioctlOk ? 1 : 0)
                << " winerr=" << r.winError
                << " scsi=0x" << Hex(r.scsiStatus, 2)
                << " sense=" << Hex(r.senseKey, 2)
                << "/" << Hex(r.asc, 2)
                << "/" << Hex(r.ascq, 2)
                << "\n";
            return false;
        }

        dump[address - kRegisterBase] = data[3];
    }

    std::cout << "RBMAXPROBE F1 result=available bytes=4096\n";
    return true;
}

bool ReadDf(
    HANDLE drive,
    ULONG alignmentMask,
    std::array<std::uint8_t, kRegisterCount>& dump) {

    constexpr std::uint32_t blockSize = 128;

    for (std::uint32_t address = kRegisterBase;
         address < kRegisterEnd;
         address += blockSize) {

        alignas(64) std::array<UCHAR, blockSize> data{};
        std::array<UCHAR, 12> cdb{};
        cdb[0] = 0xDF;
        cdb[1] = 0x85;
        cdb[3] = 0xFF;
        cdb[7] = static_cast<UCHAR>(address >> 8U);
        cdb[8] = static_cast<UCHAR>(address & 0xFFU);

        const ScsiResult r =
            SendScsi(
                drive,
                alignmentMask,
                cdb,
                SCSI_IOCTL_DATA_IN,
                data.data(),
                static_cast<ULONG>(data.size()));

        if (!r.Good()) {
            std::cout
                << "RBMAXPROBE DF result=unavailable address=0x"
                << Hex(address, 4)
                << " ioctl=" << (r.ioctlOk ? 1 : 0)
                << " winerr=" << r.winError
                << " scsi=0x" << Hex(r.scsiStatus, 2)
                << " sense=" << Hex(r.senseKey, 2)
                << "/" << Hex(r.asc, 2)
                << "/" << Hex(r.ascq, 2)
                << "\n";
            return false;
        }

        std::copy(
            data.begin(),
            data.end(),
            dump.begin() +
                static_cast<std::ptrdiff_t>(address - kRegisterBase));
    }

    std::cout << "RBMAXPROBE DF result=available bytes=4096\n";
    return true;
}

bool SaveBinary(
    const std::filesystem::path& path,
    const std::array<std::uint8_t, kRegisterCount>& dump) {

    std::ofstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }

    file.write(
        reinterpret_cast<const char*>(dump.data()),
        static_cast<std::streamsize>(dump.size()));

    return file.good();
}

void PrintChangedSignatures(
    const std::array<std::uint8_t, kRegisterCount>& dump) {

    constexpr std::array<std::uint8_t, 3> stockL0{
        0x22, 0xD8, 0x00};
    constexpr std::array<std::uint8_t, 3> stockL1{
        0x42, 0xB0, 0x00};
    constexpr std::array<std::uint8_t, 3> burnerL0{
        0x23, 0x8E, 0x10};
    constexpr std::array<std::uint8_t, 3> burnerL1{
        0x44, 0x1C, 0x20};

    const auto countPattern =
        [&](const std::array<std::uint8_t, 3>& pattern) {
            std::size_t count = 0;
            for (std::size_t i = 0;
                 i + pattern.size() <= dump.size();
                 ++i) {
                if (dump[i] == pattern[0] &&
                    dump[i + 1] == pattern[1] &&
                    dump[i + 2] == pattern[2]) {
                    ++count;
                }
            }
            return count;
        };

    std::cout
        << "RBMAXPROBE SIGNATURES"
        << " stock_l0=" << countPattern(stockL0)
        << " stock_l1=" << countPattern(stockL1)
        << " burner_l0=" << countPattern(burnerL0)
        << " burner_l1=" << countPattern(burnerL1)
        << "\n";
}

} // namespace

int wmain(int argc, wchar_t** argv) {
    std::wstring drive;
    std::filesystem::path prefix;

    for (int i = 1; i < argc; ++i) {
        const std::wstring arg = argv[i];

        if (arg == L"--drive" && i + 1 < argc) {
            drive = argv[++i];
        } else if (arg == L"--out" && i + 1 < argc) {
            prefix = argv[++i];
        } else if (arg == L"--help" || arg == L"-h") {
            std::wcout
                << L"Usage: retroburner-burnermax-probe.exe "
                << L"--drive K: --out <prefix>\n";
            return 0;
        } else {
            std::wcerr << L"Unknown/missing argument: " << arg << L"\n";
            return 2;
        }
    }

    if (drive.empty() || prefix.empty()) {
        std::wcerr << L"--drive and --out are required.\n";
        return 2;
    }

    const std::wstring devicePath = DevicePath(drive);
    if (devicePath.empty()) {
        std::wcerr << L"Invalid drive argument.\n";
        return 2;
    }

    UniqueHandle handle;
    handle.value = CreateFileW(
        devicePath.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr);

    if (handle.value == INVALID_HANDLE_VALUE) {
        std::cerr
            << "RBMAXPROBE OPEN result=failed winerr="
            << GetLastError() << "\n";
        return 3;
    }

    const ULONG alignmentMask = QueryAlignmentMask(handle.value);

    std::cout
        << "RBMAXPROBE BEGIN version=1.0"
        << " drive=" << static_cast<char>(drive[0])
        << ": alignment_mask=0x"
        << Hex(alignmentMask, 8)
        << "\n";

    ReadInquiry(handle.value, alignmentMask);

    std::uint32_t boundary = 0;
    if (!ReadLayerBoundary(handle.value, alignmentMask, boundary)) {
        std::cout << "RBMAXPROBE LAYER_BOUNDARY result=unavailable\n";
    }

    std::array<std::uint8_t, kRegisterCount> f1{};
    std::array<std::uint8_t, kRegisterCount> df{};

    const bool f1Ok = ReadF1(handle.value, alignmentMask, f1);
    const bool dfOk = ReadDf(handle.value, alignmentMask, df);

    std::error_code ec;
    std::filesystem::create_directories(prefix.parent_path(), ec);

    if (f1Ok) {
        const auto path =
            std::filesystem::path(prefix.wstring() + L"-f1.bin");
        if (!SaveBinary(path, f1)) {
            std::cerr << "RBMAXPROBE SAVE F1 failed path="
                      << path.string() << "\n";
            return 4;
        }
        PrintChangedSignatures(f1);
        std::cout << "RBMAXPROBE FILE family=F1 path="
                  << path.string() << "\n";
    }

    if (dfOk) {
        const auto path =
            std::filesystem::path(prefix.wstring() + L"-df.bin");
        if (!SaveBinary(path, df)) {
            std::cerr << "RBMAXPROBE SAVE DF failed path="
                      << path.string() << "\n";
            return 4;
        }
        PrintChangedSignatures(df);
        std::cout << "RBMAXPROBE FILE family=DF path="
                  << path.string() << "\n";
    }

    if (f1Ok && dfOk) {
        std::size_t mismatch = 0;
        for (std::size_t i = 0; i < f1.size(); ++i) {
            if (f1[i] != df[i]) {
                ++mismatch;
            }
        }
        std::cout
            << "RBMAXPROBE F1_DF_COMPARE mismatched_bytes="
            << mismatch << "\n";
    }

    std::cout
        << "RBMAXPROBE END f1=" << (f1Ok ? 1 : 0)
        << " df=" << (dfOk ? 1 : 0)
        << "\n";

    return (f1Ok || dfOk) ? 0 : 5;
}