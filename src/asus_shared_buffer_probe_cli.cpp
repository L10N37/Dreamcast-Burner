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

struct UniqueHandle {
    HANDLE value = INVALID_HANDLE_VALUE;
    ~UniqueHandle() {
        if (value != INVALID_HANDLE_VALUE) CloseHandle(value);
    }
};

struct Packet {
    SCSI_PASS_THROUGH_DIRECT sptd{};
    ULONG filler = 0;
    std::array<UCHAR, 64> sense{};
};

struct Result {
    bool ioctlOk = false;
    DWORD winerr = 0;
    UCHAR scsi = 0;
    unsigned sk = 0, asc = 0, ascq = 0;
    long long ms = 0;
    bool good() const { return ioctlOk && scsi == 0; }
};

std::string hx(unsigned v, unsigned w) {
    std::ostringstream ss;
    ss << std::uppercase << std::hex << std::setfill('0')
       << std::setw(static_cast<int>(w)) << v;
    return ss.str();
}

std::wstring drivePath(std::wstring d) {
    if (d.empty()) return {};
    wchar_t c = d[0];
    if (c >= L'a' && c <= L'z') c = static_cast<wchar_t>(c - L'a' + L'A');
    if (c < L'A' || c > L'Z') return {};
    std::wstring p = L"\\\\.\\";
    p.push_back(c);
    p.push_back(L':');
    return p;
}

ULONG alignmentMask(HANDLE h) {
    STORAGE_PROPERTY_QUERY q{};
    q.PropertyId = StorageAdapterProperty;
    q.QueryType = PropertyStandardQuery;
    alignas(64) std::array<UCHAR,512> buf{};
    DWORD got = 0;
    if (DeviceIoControl(h, IOCTL_STORAGE_QUERY_PROPERTY,
                        &q, sizeof(q), buf.data(),
                        static_cast<DWORD>(buf.size()),
                        &got, nullptr) &&
        got >= offsetof(STORAGE_ADAPTER_DESCRIPTOR, AlignmentMask) + sizeof(ULONG)) {
        auto* d = reinterpret_cast<STORAGE_ADAPTER_DESCRIPTOR*>(buf.data());
        return d->AlignmentMask;
    }
    return 4095;
}

Result sendIn(HANDLE h, ULONG mask, const std::array<UCHAR,12>& cdb,
              std::vector<UCHAR>& out) {
    Packet p{};
    p.sptd.Length = static_cast<USHORT>(sizeof(SCSI_PASS_THROUGH_DIRECT));
    p.sptd.CdbLength = 12;
    p.sptd.SenseInfoLength = static_cast<UCHAR>(p.sense.size());
    p.sptd.DataIn = SCSI_IOCTL_DATA_IN;
    p.sptd.DataTransferLength = static_cast<ULONG>(out.size());
    p.sptd.TimeOutValue = 10;
    p.sptd.SenseInfoOffset = static_cast<ULONG>(offsetof(Packet, sense));
    std::copy(cdb.begin(), cdb.end(), p.sptd.Cdb);

    std::vector<UCHAR> storage(out.size() + static_cast<std::size_t>(mask) + 1U);
    auto raw = reinterpret_cast<std::uintptr_t>(storage.data());
    auto alignedAddr = (raw + mask) & ~static_cast<std::uintptr_t>(mask);
    auto* aligned = reinterpret_cast<UCHAR*>(alignedAddr);
    std::memset(aligned, 0, out.size());
    p.sptd.DataBuffer = aligned;

    Result r{};
    DWORD ret = 0;
    auto t0 = std::chrono::steady_clock::now();
    r.ioctlOk = DeviceIoControl(h, IOCTL_SCSI_PASS_THROUGH_DIRECT,
                                &p, sizeof(p), &p, sizeof(p),
                                &ret, nullptr) != FALSE;
    auto t1 = std::chrono::steady_clock::now();
    r.ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

    if (!r.ioctlOk) {
        r.winerr = GetLastError();
        return r;
    }

    r.scsi = p.sptd.ScsiStatus;
    r.sk = p.sense[2] & 0x0F;
    r.asc = p.sense[12];
    r.ascq = p.sense[13];

    if (r.good()) {
        std::memcpy(out.data(), aligned, out.size());
    }
    return r;
}

std::string cdbHex(const std::array<UCHAR,12>& c) {
    std::ostringstream ss;
    for (std::size_t i=0; i<c.size(); ++i) {
        if (i) ss << ' ';
        ss << hx(c[i],2);
    }
    return ss.str();
}

std::string preview(const std::vector<UCHAR>& d, std::size_t n=64) {
    std::ostringstream ss;
    n = std::min(n, d.size());
    for (std::size_t i=0; i<n; ++i) {
        if (i) ss << ' ';
        ss << hx(d[i],2);
    }
    return ss.str();
}

void save(const std::filesystem::path& p, const std::vector<UCHAR>& d) {
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    if (!f) throw std::runtime_error("cannot create " + p.string());
    f.write(reinterpret_cast<const char*>(d.data()),
            static_cast<std::streamsize>(d.size()));
}

void report(const char* name, const std::array<UCHAR,12>& cdb,
            const Result& r, const std::vector<UCHAR>& data) {
    std::cout << "RBASUS79 CMD name=" << name
              << " cdb='" << cdbHex(cdb) << "'"
              << " bytes=" << data.size()
              << " ioctl=" << (r.ioctlOk ? 1 : 0)
              << " winerr=" << r.winerr
              << " scsi=0x" << hx(r.scsi,2)
              << " sense=" << hx(r.sk,2) << "/" << hx(r.asc,2) << "/" << hx(r.ascq,2)
              << " elapsed_ms=" << r.ms << "\n";
    if (r.good())
        std::cout << "RBASUS79 DATA name=" << name
                  << " preview='" << preview(data) << "'\n";
}

bool samePrefix(const std::vector<UCHAR>& a, const std::vector<UCHAR>& b, std::size_t n) {
    n = std::min({n, a.size(), b.size()});
    return std::equal(a.begin(), a.begin()+static_cast<std::ptrdiff_t>(n), b.begin());
}

std::vector<int> diffOffsets(const std::vector<UCHAR>& a,
                             const std::vector<UCHAR>& b,
                             std::size_t n=256) {
    n = std::min({n, a.size(), b.size()});
    std::vector<int> v;
    for (std::size_t i=0; i<n; ++i)
        if (a[i] != b[i]) v.push_back(static_cast<int>(i));
    return v;
}

void printDiff(const char* name, const std::vector<UCHAR>& a,
               const std::vector<UCHAR>& b) {
    auto d = diffOffsets(a,b,256);
    std::cout << "RBASUS79 DIFF name=" << name
              << " changed_first256=" << d.size();
    if (!d.empty()) {
        std::cout << " offsets=";
        for (std::size_t i=0; i<d.size() && i<64; ++i) {
            if (i) std::cout << ",";
            std::cout << "0x" << hx(static_cast<unsigned>(d[i]),4);
        }
        if (d.size() > 64) std::cout << ",...";
    }
    std::cout << "\n";
}

} // namespace

int wmain(int argc, wchar_t** argv) {
    std::wstring drive = L"L:";
    std::filesystem::path outdir = L"tools\\burnermax-dev\\asus-v79";

    for (int i=1; i<argc; ++i) {
        std::wstring a = argv[i];
        if (a == L"--drive" && i+1 < argc) drive = argv[++i];
        else if (a == L"--out" && i+1 < argc) outdir = argv[++i];
        else if (a == L"--help" || a == L"-h") {
            std::wcout << L"Usage: retroburner-asus-shared-buffer-probe.exe "
                          L"--drive L: [--out directory]\n";
            return 0;
        } else {
            std::wcerr << L"Unknown/missing argument: " << a << L"\n";
            return 2;
        }
    }

    std::error_code ec;
    std::filesystem::create_directories(outdir, ec);

    UniqueHandle h;
    auto p = drivePath(drive);
    h.value = CreateFileW(p.c_str(), GENERIC_READ | GENERIC_WRITE,
                          FILE_SHARE_READ | FILE_SHARE_WRITE,
                          nullptr, OPEN_EXISTING, 0, nullptr);
    if (h.value == INVALID_HANDLE_VALUE) {
        std::cerr << "RBASUS79 OPEN failed winerr=" << GetLastError() << "\n";
        return 3;
    }

    ULONG mask = alignmentMask(h.value);
    std::cout << "RBASUS79 BEGIN drive=" << static_cast<char>(drive[0])
              << ": alignment_mask=0x" << hx(mask,8) << "\n";

    auto doCmd = [&](const char* name, const std::array<UCHAR,12>& cdb,
                     std::size_t bytes, const char* file) {
        std::vector<UCHAR> data(bytes);
        auto r = sendIn(h.value, mask, cdb, data);
        report(name,cdb,r,data);
        if (r.good()) save(outdir / file, data);
        return std::make_pair(r,std::move(data));
    };

    std::array<UCHAR,12> df{};
    df[0] = 0xDF; df[1] = 0x02; df[2] = 0x09;

    std::array<UCHAR,12> dvd{};
    dvd[0] = 0xAD;
    dvd[7] = 0x20;
    dvd[8] = 0x08;
    dvd[9] = 0x00;

    std::array<UCHAR,12> gc{};
    gc[0] = 0x46;
    gc[7] = 0x10;
    gc[8] = 0x00;

    auto base = doCmd("DF_BEFORE", df, 65536, "01-df-before.bin");

    auto rd = doCmd("READ_DVD_STRUCTURE_20", dvd, 2048, "02-read-dvd-structure-20.bin");
    auto afterDvd = doCmd("DF_AFTER_DVD_STRUCTURE", df, 65536, "03-df-after-dvd-structure.bin");

    auto conf = doCmd("GET_CONFIGURATION", gc, 4096, "04-get-configuration.bin");
    auto afterConf = doCmd("DF_AFTER_GET_CONFIGURATION", df, 65536, "05-df-after-get-configuration.bin");

    if (base.first.good() && afterDvd.first.good())
        printDiff("DF_BEFORE_vs_AFTER_DVD_STRUCTURE", base.second, afterDvd.second);

    if (afterDvd.first.good() && afterConf.first.good())
        printDiff("DF_AFTER_DVD_STRUCTURE_vs_AFTER_GET_CONFIGURATION", afterDvd.second, afterConf.second);

    if (rd.first.good() && afterDvd.first.good()) {
        std::cout << "RBASUS79 MATCH dvd_structure_prefix="
                  << (samePrefix(rd.second, afterDvd.second, 128) ? 1 : 0) << "\n";
    }

    if (conf.first.good() && afterConf.first.good()) {
        std::cout << "RBASUS79 MATCH get_configuration_prefix="
                  << (samePrefix(conf.second, afterConf.second, 256) ? 1 : 0) << "\n";
    }

    std::cout << "RBASUS79 END\n";
    std::cout << "RBASUS79 SAFETY data_in_only=1 media_write=0 register_write=0 "
                 "firmware_write=0 mode_select=0 opc=0 set_streaming=0\n";
    return 0;
}