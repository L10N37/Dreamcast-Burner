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
        if (value != INVALID_HANDLE_VALUE) CloseHandle(value);
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
    bool Good() const { return ioctlOk && scsiStatus == 0; }
};

std::string Hex(unsigned value, unsigned width) {
    std::ostringstream ss;
    ss << std::uppercase << std::hex << std::setfill('0')
       << std::setw(static_cast<int>(width)) << value;
    return ss.str();
}

std::wstring DevicePath(const std::wstring& driveArg) {
    if (driveArg.empty()) return {};
    wchar_t letter = driveArg[0];
    if (letter >= L'a' && letter <= L'z')
        letter = static_cast<wchar_t>(letter - L'a' + L'A');
    if (letter < L'A' || letter > L'Z') return {};
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
            drive, IOCTL_STORAGE_QUERY_PROPERTY,
            &query, sizeof(query),
            buffer.data(), static_cast<DWORD>(buffer.size()),
            &returned, nullptr) &&
        returned >= offsetof(STORAGE_ADAPTER_DESCRIPTOR, AlignmentMask) + sizeof(ULONG)) {
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
    void* data,
    ULONG dataLength) {

    ScsiPacket packet{};
    packet.sptd.Length = static_cast<USHORT>(sizeof(SCSI_PASS_THROUGH_DIRECT));
    packet.sptd.CdbLength = 12;
    packet.sptd.SenseInfoLength = static_cast<UCHAR>(packet.sense.size());
    packet.sptd.DataIn = SCSI_IOCTL_DATA_IN;
    packet.sptd.DataTransferLength = dataLength;
    packet.sptd.TimeOutValue = kTimeoutSeconds;
    packet.sptd.SenseInfoOffset =
        static_cast<ULONG>(offsetof(ScsiPacket, sense));
    std::copy(cdb.begin(), cdb.end(), packet.sptd.Cdb);

    std::vector<UCHAR> storage(
        static_cast<std::size_t>(dataLength) +
        static_cast<std::size_t>(alignmentMask) + 1U);

    const std::uintptr_t raw =
        reinterpret_cast<std::uintptr_t>(storage.data());
    const std::uintptr_t alignedAddress =
        (raw + alignmentMask) &
        ~static_cast<std::uintptr_t>(alignmentMask);
    auto* aligned = reinterpret_cast<UCHAR*>(alignedAddress);
    std::memset(aligned, 0, dataLength);

    packet.sptd.DataBuffer = aligned;

    ScsiResult result{};
    DWORD returned = 0;
    const auto begin = std::chrono::steady_clock::now();

    result.ioctlOk =
        DeviceIoControl(
            drive,
            IOCTL_SCSI_PASS_THROUGH_DIRECT,
            &packet, sizeof(packet),
            &packet, sizeof(packet),
            &returned, nullptr) != FALSE;

    const auto end = std::chrono::steady_clock::now();
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

    if (result.Good() && data && dataLength) {
        const ULONG transferred =
            std::min(dataLength, packet.sptd.DataTransferLength);
        std::memcpy(data, aligned, transferred);
    }

    return result;
}

std::string CdbHex(const std::array<UCHAR,12>& cdb) {
    std::ostringstream ss;
    for (std::size_t i=0; i<cdb.size(); ++i) {
        if (i) ss << ' ';
        ss << Hex(cdb[i], 2);
    }
    return ss.str();
}

std::string DataPreview(const std::vector<UCHAR>& data, std::size_t count = 32) {
    std::ostringstream ss;
    const std::size_t n = std::min(count, data.size());
    for (std::size_t i=0; i<n; ++i) {
        if (i) ss << ' ';
        ss << Hex(data[i], 2);
    }
    return ss.str();
}

bool SaveBinary(
    const std::filesystem::path& prefix,
    const std::string& name,
    const std::vector<UCHAR>& data) {

    if (prefix.empty()) return true;

    std::filesystem::path path =
        prefix.string() + "-" + name + ".bin";

    std::error_code ec;
    if (path.has_parent_path())
        std::filesystem::create_directories(path.parent_path(), ec);

    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) {
        std::cerr << "RBASUS77 FILE result=failed name=" << name
                  << " path='" << path.string() << "'\n";
        return false;
    }

    f.write(
        reinterpret_cast<const char*>(data.data()),
        static_cast<std::streamsize>(data.size()));
    f.close();

    std::cout << "RBASUS77 FILE result=ok name=" << name
              << " bytes=" << data.size()
              << " path='" << path.string() << "'\n";
    return true;
}

bool Probe(
    HANDLE drive,
    ULONG alignmentMask,
    const char* name,
    const std::array<UCHAR,12>& cdb,
    ULONG bytes,
    const std::filesystem::path& dumpPrefix,
    bool saveOnGood = false) {

    std::vector<UCHAR> data(bytes);
    const auto r =
        SendScsiIn(drive, alignmentMask, cdb, data.data(), bytes);

    std::cout
        << "RBASUS77 CMD name=" << name
        << " cdb='" << CdbHex(cdb) << "'"
        << " bytes=" << bytes
        << " ioctl=" << (r.ioctlOk ? 1 : 0)
        << " winerr=" << r.winError
        << " scsi=0x" << Hex(r.scsiStatus,2)
        << " sense=" << Hex(r.senseKey,2)
        << "/" << Hex(r.asc,2)
        << "/" << Hex(r.ascq,2)
        << " elapsed_ms=" << r.elapsedMs
        << "\n";

    if (r.Good()) {
        std::size_t nonzero = 0;
        for (auto b : data) if (b != 0) ++nonzero;
        std::cout
            << "RBASUS77 DATA name=" << name
            << " nonzero=" << nonzero
            << " preview='" << DataPreview(data) << "'\n";

        if (saveOnGood && !dumpPrefix.empty()) {
            if (!SaveBinary(dumpPrefix, name, data))
                return false;
        }
    }
    return true;
}

} // namespace

int wmain(int argc, wchar_t** argv) {
    std::wstring drive = L"L:";
    std::filesystem::path dumpPrefix;
    bool focused = false;

    for (int i=1; i<argc; ++i) {
        std::wstring arg = argv[i];

        if (arg == L"--drive" && i+1 < argc) {
            drive = argv[++i];
        } else if (arg == L"--out" && i+1 < argc) {
            dumpPrefix = argv[++i];
        } else if (arg == L"--focused") {
            focused = true;
        } else if (arg == L"--help" || arg == L"-h") {
            std::wcout
                << L"Usage: retroburner-asus-family-probe.exe "
                   L"--drive L: [--out prefix] [--focused]\n";
            return 0;
        } else {
            std::wcerr << L"Unknown/missing argument: " << arg << L"\n";
            return 2;
        }
    }

    const auto path = DevicePath(drive);
    if (path.empty()) {
        std::wcerr << L"Invalid drive.\n";
        return 2;
    }

    UniqueHandle h;
    h.value = CreateFileW(
        path.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, OPEN_EXISTING, 0, nullptr);

    if (h.value == INVALID_HANDLE_VALUE) {
        std::cerr << "RBASUS77 OPEN failed winerr=" << GetLastError() << "\n";
        return 3;
    }

    const ULONG mask = QueryAlignmentMask(h.value);

    std::cout
        << "RBASUS77 BEGIN version=1.1 drive="
        << static_cast<char>(drive[0])
        << ": alignment_mask=0x" << Hex(mask,8)
        << " focused=" << (focused ? 1 : 0)
        << "\n";

    if (!focused) {
        {
            std::array<UCHAR,12> cdb{};
            cdb[0] = 0x12;
            cdb[4] = 96;
            if (!Probe(h.value, mask, "INQUIRY", cdb, 96, dumpPrefix, false))
                return 4;
        }

        {
            std::array<UCHAR,12> cdb{};
            cdb[0] = 0x5A;
            cdb[2] = 0x38;
            cdb[3] = 0x41;
            cdb[4] = 0x53;
            cdb[5] = 0x10;
            cdb[9] = 0x04;
            if (!Probe(h.value, mask, "QPX_ASUS_5A_38_AS", cdb, 20, dumpPrefix, true))
                return 4;
        }

        {
            std::array<UCHAR,12> cdb{};
            cdb[0] = 0xF3;
            cdb[1] = 0x0E;
            cdb[8] = 0x10;
            if (!Probe(h.value, mask, "LITEON_F3_0E", cdb, 0x10, dumpPrefix, true))
                return 4;
        }
    }

    {
        std::array<UCHAR,12> cdb{};
        cdb[0] = 0xDF;
        cdb[1] = 0x02;
        cdb[2] = 0x09;
        if (!Probe(h.value, mask, "LITEON_DF_02_09", cdb, 65536, dumpPrefix, true))
            return 4;
    }

    if (!focused) {
        {
            std::array<UCHAR,12> cdb{};
            cdb[0] = 0xDF;
            cdb[1] = 0x08;
            cdb[2] = 0x02;
            if (!Probe(h.value, mask, "LITEON_DF_08_02", cdb, 65536, dumpPrefix, true))
                return 4;
        }
    }

    {
        std::array<UCHAR,12> cdb{};
        cdb[0] = 0xDF;
        cdb[1] = 0x82;
        cdb[2] = 0x09;
        if (!Probe(h.value, mask, "LITEON_DF_82_09", cdb, 256, dumpPrefix, true))
            return 4;
    }

    if (!focused) {
        {
            std::array<UCHAR,12> cdb{};
            cdb[0] = 0xDF;
            cdb[1] = 0x97;
            if (!Probe(h.value, mask, "LITEON_DF_97", cdb, 256, dumpPrefix, true))
                return 4;
        }
    }

    std::cout << "RBASUS77 END\n";
    std::cout
        << "RBASUS77 SAFETY data_in_only=1 "
        << "media_write=0 register_write=0 firmware_write=0 "
        << "mode_select=0 opc=0 set_streaming=0\n";

    return 0;
}