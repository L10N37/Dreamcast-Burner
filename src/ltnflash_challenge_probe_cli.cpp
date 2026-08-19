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
    std::array<UCHAR,64> sense{};
};

struct Result {
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

std::string Hex(unsigned v, unsigned width) {
    std::ostringstream ss;
    ss << std::uppercase << std::hex << std::setfill('0')
       << std::setw(static_cast<int>(width)) << v;
    return ss.str();
}

std::wstring DevicePath(const std::wstring& driveArg) {
    if (driveArg.empty()) return {};
    wchar_t c = driveArg[0];
    if (c >= L'a' && c <= L'z')
        c = static_cast<wchar_t>(c - L'a' + L'A');
    if (c < L'A' || c > L'Z')
        return {};

    std::wstring p = L"\\\\.\\";
    p.push_back(c);
    p.push_back(L':');
    return p;
}

ULONG QueryAlignmentMask(HANDLE drive) {
    STORAGE_PROPERTY_QUERY q{};
    q.PropertyId = StorageAdapterProperty;
    q.QueryType = PropertyStandardQuery;

    alignas(64) std::array<UCHAR,512> buf{};
    DWORD returned = 0;

    if (DeviceIoControl(
            drive,
            IOCTL_STORAGE_QUERY_PROPERTY,
            &q, sizeof(q),
            buf.data(), static_cast<DWORD>(buf.size()),
            &returned, nullptr) &&
        returned >= offsetof(STORAGE_ADAPTER_DESCRIPTOR, AlignmentMask) + sizeof(ULONG)) {

        const auto* d =
            reinterpret_cast<const STORAGE_ADAPTER_DESCRIPTOR*>(buf.data());

        return d->AlignmentMask;
    }

    return 4095;
}

Result SendIn(
    HANDLE drive,
    ULONG mask,
    const std::array<UCHAR,12>& cdb,
    std::vector<UCHAR>& data) {

    ScsiPacket p{};
    p.sptd.Length = static_cast<USHORT>(sizeof(SCSI_PASS_THROUGH_DIRECT));
    p.sptd.CdbLength = 12;
    p.sptd.SenseInfoLength = static_cast<UCHAR>(p.sense.size());
    p.sptd.DataIn = SCSI_IOCTL_DATA_IN;
    p.sptd.DataTransferLength = static_cast<ULONG>(data.size());
    p.sptd.TimeOutValue = kTimeoutSeconds;
    p.sptd.SenseInfoOffset = static_cast<ULONG>(offsetof(ScsiPacket,sense));
    std::copy(cdb.begin(), cdb.end(), p.sptd.Cdb);

    std::vector<UCHAR> storage(
        data.size() + static_cast<std::size_t>(mask) + 1U);

    const auto raw =
        reinterpret_cast<std::uintptr_t>(storage.data());

    const auto alignedAddress =
        (raw + mask) & ~static_cast<std::uintptr_t>(mask);

    auto* aligned =
        reinterpret_cast<UCHAR*>(alignedAddress);

    std::memset(aligned, 0, data.size());
    p.sptd.DataBuffer = aligned;

    Result r{};
    DWORD returned = 0;
    const auto t0 = std::chrono::steady_clock::now();

    r.ioctlOk =
        DeviceIoControl(
            drive,
            IOCTL_SCSI_PASS_THROUGH_DIRECT,
            &p, sizeof(p),
            &p, sizeof(p),
            &returned,
            nullptr) != FALSE;

    const auto t1 = std::chrono::steady_clock::now();

    r.elapsedMs =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            t1 - t0).count();

    if (!r.ioctlOk) {
        r.winError = GetLastError();
        return r;
    }

    r.scsiStatus = p.sptd.ScsiStatus;
    r.senseKey = p.sense[2] & 0x0F;
    r.asc = p.sense[12];
    r.ascq = p.sense[13];

    if (r.Good()) {
        const ULONG transferred =
            std::min(
                static_cast<ULONG>(data.size()),
                p.sptd.DataTransferLength);

        std::memcpy(data.data(), aligned, transferred);
    }

    return r;
}

std::string CdbHex(const std::array<UCHAR,12>& cdb) {
    std::ostringstream ss;
    for (std::size_t i=0; i<cdb.size(); ++i) {
        if (i) ss << ' ';
        ss << Hex(cdb[i],2);
    }
    return ss.str();
}

std::string Preview(const std::vector<UCHAR>& data, std::size_t count=128) {
    std::ostringstream ss;
    const auto n = std::min(count, data.size());

    for (std::size_t i=0; i<n; ++i) {
        if (i) ss << ' ';
        ss << Hex(data[i],2);
    }

    return ss.str();
}

bool SaveBinary(
    const std::filesystem::path& path,
    const std::vector<UCHAR>& data) {

    if (path.empty()) return true;

    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) return false;

    f.write(
        reinterpret_cast<const char*>(data.data()),
        static_cast<std::streamsize>(data.size()));

    return static_cast<bool>(f);
}

Result Probe(
    HANDLE drive,
    ULONG mask,
    const char* name,
    const std::array<UCHAR,12>& cdb,
    std::size_t bytes,
    const std::filesystem::path& file = {}) {

    std::vector<UCHAR> data(bytes,0);
    const auto r = SendIn(drive,mask,cdb,data);

    std::cout
        << "RBASUS82 CMD"
        << " name=" << name
        << " cdb='" << CdbHex(cdb) << "'"
        << " bytes=" << bytes
        << " direction=IN"
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
        for (auto b : data)
            if (b != 0) ++nonzero;

        std::cout
            << "RBASUS82 DATA"
            << " name=" << name
            << " nonzero=" << nonzero
            << " preview='" << Preview(data) << "'"
            << "\n";

        if (std::string(name) == "LTN_REG_CHALLENGE") {
            std::cout
                << "RBASUS82 CHALLENGE"
                << " byte32=0x" << Hex(data[32],2)
                << " byte33=0x" << Hex(data[33],2)
                << "\n";
        }

        if (!file.empty()) {
            const bool ok = SaveBinary(file,data);
            std::cout
                << "RBASUS82 FILE"
                << " result=" << (ok ? "ok" : "failed")
                << " path='" << file.string() << "'"
                << "\n";
        }
    }

    return r;
}

} // namespace

int wmain(int argc, wchar_t** argv) {
    std::wstring drive = L"L:";
    std::filesystem::path outDir;

    for (int i=1; i<argc; ++i) {
        const std::wstring arg = argv[i];

        if (arg == L"--drive" && i+1 < argc) {
            drive = argv[++i];
        } else if (arg == L"--out" && i+1 < argc) {
            outDir = argv[++i];
        } else if (arg == L"--help" || arg == L"-h") {
            std::wcout
                << L"Usage: retroburner-ltnflash-challenge-probe.exe "
                   L"--drive L: [--out directory]\n";
            return 0;
        } else {
            std::wcerr << L"Unknown/missing argument: " << arg << L"\n";
            return 2;
        }
    }

    const auto path = DevicePath(drive);
    if (path.empty()) {
        std::cerr << "RBASUS82 invalid drive\n";
        return 2;
    }

    if (!outDir.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(outDir,ec);
        if (ec) {
            std::cerr << "RBASUS82 cannot create output directory\n";
            return 3;
        }
    }

    UniqueHandle h;
    h.value =
        CreateFileW(
            path.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_EXISTING,
            0,
            nullptr);

    if (h.value == INVALID_HANDLE_VALUE) {
        std::cerr
            << "RBASUS82 OPEN failed winerr="
            << GetLastError()
            << "\n";
        return 3;
    }

    const ULONG mask = QueryAlignmentMask(h.value);

    std::cout
        << "RBASUS82 BEGIN"
        << " version=1.0"
        << " drive=" << static_cast<char>(drive[0]) << ":"
        << " alignment_mask=0x" << Hex(mask,8)
        << "\n";

    // Recovered from LtnFlash 2.6.11 embedded MFlashPI.class:
    // getChallengeKey():
    //   DF 00 0A 00 00 00 00 00 00 00 00 00
    //   128 bytes DATA-IN.
    //
    // This is the read half of LtnFlash's REG/service handshake.
    std::array<UCHAR,12> challenge{};
    challenge[0] = 0xDF;
    challenge[1] = 0x00;
    challenge[2] = 0x0A;
    challenge[3] = 0x00;

    // Proven LtnFlash readMemory(), program-memory selector used by firmware verify.
    std::array<UCHAR,12> readMemory{};
    readMemory[0] = 0xDF;
    readMemory[1] = 0x83;
    readMemory[4] = 0xFF;

    // Proven LtnFlash readEEPROM().
    std::array<UCHAR,12> readEeprom{};
    readEeprom[0] = 0xDF;
    readEeprom[1] = 0x89;

    const auto challengeFile =
        outDir.empty()
        ? std::filesystem::path{}
        : outDir / "df00-0a-challenge.bin";

    const auto memoryFile =
        outDir.empty()
        ? std::filesystem::path{}
        : outDir / "df83-after-challenge.bin";

    const auto eepromFile =
        outDir.empty()
        ? std::filesystem::path{}
        : outDir / "df89-after-challenge.bin";

    const auto rc =
        Probe(
            h.value,
            mask,
            "LTN_REG_CHALLENGE",
            challenge,
            128,
            challengeFile);

    const auto rm =
        Probe(
            h.value,
            mask,
            "LTN_DF83_AFTER_CHALLENGE",
            readMemory,
            128,
            memoryFile);

    const auto re =
        Probe(
            h.value,
            mask,
            "LTN_DF89_AFTER_CHALLENGE",
            readEeprom,
            128,
            eepromFile);

    std::cout
        << "RBASUS82 RESULT"
        << " challenge=" << (rc.Good() ? "GOOD" : "NOT_GOOD")
        << " df83_after=" << (rm.Good() ? "GOOD" : "NOT_GOOD")
        << " df89_after=" << (re.Good() ? "GOOD" : "NOT_GOOD")
        << "\n";

    std::cout
        << "RBASUS82 SAFETY"
        << " data_in_only=1"
        << " data_out=0"
        << " media_write=0"
        << " register_write=0"
        << " firmware_write=0"
        << " flash_write=0"
        << " eeprom_write=0"
        << " opc=0"
        << " set_streaming=0"
        << " mode_select=0"
        << " write_buffer=0"
        << " note='challenge may generate/change volatile session nonce only'"
        << "\n";

    return 0;
}