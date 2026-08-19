#include <windows.h>
#include <winioctl.h>
#include <ntddscsi.h>
#include <ntddstor.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr ULONG kTimeoutSeconds = 10;

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
    ss << std::uppercase
       << std::hex
       << std::setfill('0')
       << std::setw(static_cast<int>(width))
       << value;
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
        returned >=
            offsetof(STORAGE_ADAPTER_DESCRIPTOR, AlignmentMask) + sizeof(ULONG)) {

        const auto* descriptor =
            reinterpret_cast<const STORAGE_ADAPTER_DESCRIPTOR*>(buffer.data());
        return descriptor->AlignmentMask;
    }

    return 4095;
}

ScsiResult SendScsiIn(
    HANDLE drive,
    ULONG alignmentMask,
    const std::array<UCHAR, 12>& cdb,
    std::vector<UCHAR>& data) {

    ScsiPacket packet{};

    packet.sptd.Length =
        static_cast<USHORT>(sizeof(SCSI_PASS_THROUGH_DIRECT));
    packet.sptd.CdbLength = 12;
    packet.sptd.SenseInfoLength =
        static_cast<UCHAR>(packet.sense.size());
    packet.sptd.DataIn = SCSI_IOCTL_DATA_IN;
    packet.sptd.DataTransferLength =
        static_cast<ULONG>(data.size());
    packet.sptd.TimeOutValue = kTimeoutSeconds;
    packet.sptd.SenseInfoOffset =
        static_cast<ULONG>(offsetof(ScsiPacket, sense));

    std::copy(cdb.begin(), cdb.end(), packet.sptd.Cdb);

    std::vector<UCHAR> storage(
        data.size() +
        static_cast<std::size_t>(alignmentMask) +
        1U);

    const std::uintptr_t raw =
        reinterpret_cast<std::uintptr_t>(storage.data());

    const std::uintptr_t alignedAddress =
        (raw + alignmentMask) &
        ~static_cast<std::uintptr_t>(alignmentMask);

    auto* aligned =
        reinterpret_cast<UCHAR*>(alignedAddress);

    std::memset(aligned, 0, data.size());
    packet.sptd.DataBuffer = aligned;

    ScsiResult result{};
    DWORD returned = 0;

    const auto begin =
        std::chrono::steady_clock::now();

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

    const auto end =
        std::chrono::steady_clock::now();

    result.elapsedMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            end - begin).count();

    if (!result.ioctlOk) {
        result.winError = GetLastError();
        return result;
    }

    result.scsiStatus = packet.sptd.ScsiStatus;
    result.senseKey = packet.sense[2] & 0x0F;
    result.asc = packet.sense[12];
    result.ascq = packet.sense[13];

    if (result.Good()) {
        const ULONG transferred =
            std::min(
                static_cast<ULONG>(data.size()),
                packet.sptd.DataTransferLength);

        std::memcpy(
            data.data(),
            aligned,
            transferred);
    }

    return result;
}

std::string CdbHex(const std::array<UCHAR, 12>& cdb) {
    std::ostringstream ss;

    for (std::size_t i = 0; i < cdb.size(); ++i) {
        if (i) {
            ss << ' ';
        }
        ss << Hex(cdb[i], 2);
    }

    return ss.str();
}

std::string Preview(
    const std::vector<UCHAR>& data,
    std::size_t count = 128) {

    std::ostringstream ss;

    const std::size_t n =
        std::min(count, data.size());

    for (std::size_t i = 0; i < n; ++i) {
        if (i) {
            ss << ' ';
        }
        ss << Hex(data[i], 2);
    }

    return ss.str();
}

bool SaveBinary(
    const std::filesystem::path& path,
    const std::vector<UCHAR>& data) {

    std::ofstream f(
        path,
        std::ios::binary | std::ios::trunc);

    if (!f) {
        return false;
    }

    f.write(
        reinterpret_cast<const char*>(data.data()),
        static_cast<std::streamsize>(data.size()));

    return static_cast<bool>(f);
}

bool Probe(
    HANDLE drive,
    ULONG alignmentMask,
    const char* name,
    const std::array<UCHAR, 12>& cdb,
    std::size_t bytes,
    const std::filesystem::path& outputPath) {

    std::vector<UCHAR> data(bytes, 0);

    const auto r =
        SendScsiIn(
            drive,
            alignmentMask,
            cdb,
            data);

    std::cout
        << "RBASUS81 CMD"
        << " name=" << name
        << " cdb='" << CdbHex(cdb) << "'"
        << " bytes=" << bytes
        << " direction=IN"
        << " ioctl=" << (r.ioctlOk ? 1 : 0)
        << " winerr=" << r.winError
        << " scsi=0x" << Hex(r.scsiStatus, 2)
        << " sense="
        << Hex(r.senseKey, 2)
        << "/"
        << Hex(r.asc, 2)
        << "/"
        << Hex(r.ascq, 2)
        << " elapsed_ms=" << r.elapsedMs
        << "\n";

    if (!r.Good()) {
        return false;
    }

    std::size_t nonzero = 0;
    for (const auto b : data) {
        if (b != 0) {
            ++nonzero;
        }
    }

    std::cout
        << "RBASUS81 DATA"
        << " name=" << name
        << " nonzero=" << nonzero
        << " preview='" << Preview(data) << "'"
        << "\n";

    if (!outputPath.empty()) {
        if (!SaveBinary(outputPath, data)) {
            std::cerr
                << "RBASUS81 FILE result=failed"
                << " path='" << outputPath.string() << "'"
                << "\n";
            return false;
        }

        std::cout
            << "RBASUS81 FILE result=ok"
            << " bytes=" << data.size()
            << " path='" << outputPath.string() << "'"
            << "\n";
    }

    return true;
}

} // namespace

int wmain(int argc, wchar_t** argv) {
    std::wstring drive = L"L:";
    std::filesystem::path outDir;

    for (int i = 1; i < argc; ++i) {
        const std::wstring arg = argv[i];

        if (arg == L"--drive" && i + 1 < argc) {
            drive = argv[++i];
        } else if (arg == L"--out" && i + 1 < argc) {
            outDir = argv[++i];
        } else if (arg == L"--help" || arg == L"-h") {
            std::wcout
                << L"Usage: retroburner-ltnflash-read-probe.exe "
                   L"--drive L: [--out directory]\n";
            return 0;
        } else {
            std::wcerr
                << L"Unknown/missing argument: "
                << arg
                << L"\n";
            return 2;
        }
    }

    const auto path = DevicePath(drive);
    if (path.empty()) {
        std::wcerr << L"Invalid drive.\n";
        return 2;
    }

    if (!outDir.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(
            outDir,
            ec);

        if (ec) {
            std::cerr
                << "RBASUS81 cannot create output directory\n";
            return 3;
        }
    }

    UniqueHandle handle;

    // Windows SPTI commonly requires read/write access to the device handle
    // even for DATA-IN-only pass-through commands.  The CDBs below are the
    // actual safety boundary: this target contains only DATA-IN probes.
    handle.value =
        CreateFileW(
            path.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_EXISTING,
            0,
            nullptr);

    if (handle.value == INVALID_HANDLE_VALUE) {
        std::cerr
            << "RBASUS81 OPEN failed"
            << " winerr=" << GetLastError()
            << "\n";
        return 3;
    }

    const ULONG mask =
        QueryAlignmentMask(handle.value);

    std::cout
        << "RBASUS81 BEGIN"
        << " version=1.0"
        << " drive=" << static_cast<char>(drive[0]) << ":"
        << " alignment_mask=0x" << Hex(mask, 8)
        << "\n";

    // LtnFlash 2.6.11 embedded MFlashPI.class:
    //
    // readMemory(SCmdsAll, memoryType, bank, hiAddr, loAddr)
    //   cdb[0] = 0xDF
    //   cdb[1] = 0x83
    //   cdb[4] = memoryType
    //   cdb[6] = bank
    //   cdb[7] = hiAddr
    //   cdb[8] = loAddr
    //   transfer = 128 bytes DATA-IN
    //
    // LtnFlash firmware verification uses memoryType = 0xFF.
    std::array<UCHAR, 12> readMemory{};
    readMemory[0] = 0xDF;
    readMemory[1] = 0x83;
    readMemory[4] = 0xFF;
    readMemory[6] = 0x00;
    readMemory[7] = 0x00;
    readMemory[8] = 0x00;

    // LtnFlash 2.6.11 embedded MFlashPI.class:
    //
    // readEEPROM(SCmdsAll, hiAddr, loAddr)
    //   cdb[0] = 0xDF
    //   cdb[1] = 0x89
    //   cdb[7] = hiAddr
    //   cdb[8] = loAddr
    //   transfer = 128 bytes DATA-IN
    std::array<UCHAR, 12> readEeprom{};
    readEeprom[0] = 0xDF;
    readEeprom[1] = 0x89;

    const auto memPath =
        outDir.empty()
            ? std::filesystem::path{}
            : outDir / "df83-memory-ff-bank00-addr0000.bin";

    const auto eepromPath =
        outDir.empty()
            ? std::filesystem::path{}
            : outDir / "df89-eeprom-addr0000.bin";

    const bool memGood =
        Probe(
            handle.value,
            mask,
            "LTN_DF83_MEMORY_FF_000000",
            readMemory,
            128,
            memPath);

    const bool eepromGood =
        Probe(
            handle.value,
            mask,
            "LTN_DF89_EEPROM_0000",
            readEeprom,
            128,
            eepromPath);

    std::cout
        << "RBASUS81 RESULT"
        << " df83=" << (memGood ? "GOOD" : "NOT_GOOD")
        << " df89=" << (eepromGood ? "GOOD" : "NOT_GOOD")
        << "\n";

    std::cout
        << "RBASUS81 SAFETY"
        << " data_in_only=1"
        << " media_write=0"
        << " register_write=0"
        << " firmware_write=0"
        << " flash_write=0"
        << " eeprom_write=0"
        << " opc=0"
        << " set_streaming=0"
        << " mode_select=0"
        << " write_buffer=0"
        << "\n";

    return 0;
}