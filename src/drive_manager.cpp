#include "drive_manager.h"
#include "embedded_tools.h"

#include <windows.h>
#include <winioctl.h>
#include <ntddcdrm.h>
#include <ntddscsi.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <cwctype>
#include <string>
#include <unordered_set>
#include <utility>
#include <regex>
#include <filesystem>
#include <sstream>
#include <vector>

namespace {

constexpr float kCdOneXKilobytesPerSecond = 176.4F;
constexpr std::size_t kSenseLength = 32;

class UniqueHandle final {
public:
    explicit UniqueHandle(HANDLE handle = INVALID_HANDLE_VALUE) noexcept
        : handle_(handle) {}

    ~UniqueHandle() {
        if (IsValid()) {
            CloseHandle(handle_);
        }
    }

    UniqueHandle(const UniqueHandle&) = delete;
    UniqueHandle& operator=(const UniqueHandle&) = delete;

    UniqueHandle(UniqueHandle&& other) noexcept : handle_(other.handle_) {
        other.handle_ = INVALID_HANDLE_VALUE;
    }

    UniqueHandle& operator=(UniqueHandle&& other) noexcept {
        if (this != &other) {
            if (IsValid()) {
                CloseHandle(handle_);
            }
            handle_ = other.handle_;
            other.handle_ = INVALID_HANDLE_VALUE;
        }
        return *this;
    }

    [[nodiscard]] HANDLE Get() const noexcept { return handle_; }
    [[nodiscard]] bool IsValid() const noexcept {
        return handle_ != INVALID_HANDLE_VALUE && handle_ != nullptr;
    }

private:
    HANDLE handle_;
};

[[nodiscard]] UniqueHandle OpenDrive(const std::wstring& devicePath) {
    constexpr DWORD shareMode = FILE_SHARE_READ | FILE_SHARE_WRITE;
    HANDLE handle = CreateFileW(
        devicePath.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        shareMode,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);

    if (handle == INVALID_HANDLE_VALUE) {
        handle = CreateFileW(
            devicePath.c_str(),
            GENERIC_READ,
            shareMode,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
    }
    if (handle == INVALID_HANDLE_VALUE) {
        handle = CreateFileW(
            devicePath.c_str(),
            0,
            shareMode,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
    }
    return UniqueHandle(handle);
}

[[nodiscard]] std::string TrimAscii(std::string value) {
    const auto isNotSpace = [](const unsigned char character) {
        return std::isspace(character) == 0;
    };

    const auto first = std::find_if(value.begin(), value.end(), isNotSpace);
    const auto last = std::find_if(value.rbegin(), value.rend(), isNotSpace).base();
    if (first >= last) {
        return {};
    }
    return std::string(first, last);
}

[[nodiscard]] std::string ReadDescriptorString(
    const std::vector<std::uint8_t>& data,
    const DWORD offset) {
    if (offset == 0 || offset >= data.size()) {
        return {};
    }

    const char* begin = reinterpret_cast<const char*>(data.data() + offset);
    const std::size_t available = data.size() - offset;
    const std::size_t length = strnlen_s(begin, available);
    return TrimAscii(std::string(begin, length));
}

[[nodiscard]] std::uint16_t ReadBigEndian16(const std::uint8_t* value) noexcept {
    return static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(value[0]) << 8U) |
        static_cast<std::uint16_t>(value[1]));
}

[[nodiscard]] std::uint32_t ReadBigEndian32(const std::uint8_t* value) noexcept {
    return (static_cast<std::uint32_t>(value[0]) << 24U) |
           (static_cast<std::uint32_t>(value[1]) << 16U) |
           (static_cast<std::uint32_t>(value[2]) << 8U) |
           static_cast<std::uint32_t>(value[3]);
}

[[nodiscard]] std::string BusName(const STORAGE_BUS_TYPE busType) {
    switch (busType) {
    case BusTypeScsi: return "SCSI";
    case BusTypeAtapi: return "ATAPI";
    case BusTypeAta: return "ATA";
    case BusType1394: return "FireWire";
    case BusTypeSsa: return "SSA";
    case BusTypeFibre: return "Fibre";
    case BusTypeUsb: return "USB";
    case BusTypeRAID: return "RAID";
    case BusTypeSas: return "SAS";
    case BusTypeSata: return "SATA";
    case BusTypeSd: return "SD";
    case BusTypeMmc: return "MMC";
    case BusTypeVirtual: return "Virtual";
    case BusTypeFileBackedVirtual: return "Virtual file";
    case BusTypeSpaces: return "Storage Spaces";
    case BusTypeNvme: return "NVMe";
    default: return "Unknown";
    }
}

[[nodiscard]] std::string Win32ErrorMessage(const DWORD error) {
    wchar_t* wideMessage = nullptr;
    const DWORD length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
            FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        error,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<wchar_t*>(&wideMessage),
        0,
        nullptr);

    std::string result;
    if (length > 0 && wideMessage != nullptr) {
        const int utf8Length = WideCharToMultiByte(
            CP_UTF8, 0, wideMessage, static_cast<int>(length),
            nullptr, 0, nullptr, nullptr);
        if (utf8Length > 0) {
            result.resize(static_cast<std::size_t>(utf8Length));
            WideCharToMultiByte(
                CP_UTF8, 0, wideMessage, static_cast<int>(length),
                result.data(), utf8Length, nullptr, nullptr);
            result = TrimAscii(std::move(result));
        }
        LocalFree(wideMessage);
    }
    if (result.empty()) {
        result = "Windows error " + std::to_string(error);
    }
    return result;
}

void QueryIdentity(const HANDLE handle, OpticalDrive& drive) {
    STORAGE_PROPERTY_QUERY query{};
    query.PropertyId = StorageDeviceProperty;
    query.QueryType = PropertyStandardQuery;

    STORAGE_DESCRIPTOR_HEADER header{};
    DWORD bytesReturned = 0;
    if (!DeviceIoControl(
            handle,
            IOCTL_STORAGE_QUERY_PROPERTY,
            &query,
            sizeof(query),
            &header,
            sizeof(header),
            &bytesReturned,
            nullptr) ||
        header.Size < sizeof(STORAGE_DEVICE_DESCRIPTOR) ||
        header.Size > 64U * 1024U) {
        return;
    }

    std::vector<std::uint8_t> data(header.Size);
    if (!DeviceIoControl(
            handle,
            IOCTL_STORAGE_QUERY_PROPERTY,
            &query,
            sizeof(query),
            data.data(),
            static_cast<DWORD>(data.size()),
            &bytesReturned,
            nullptr) ||
        bytesReturned < sizeof(STORAGE_DEVICE_DESCRIPTOR)) {
        return;
    }

    data.resize(bytesReturned);
    const auto* descriptor =
        reinterpret_cast<const STORAGE_DEVICE_DESCRIPTOR*>(data.data());
    drive.vendor = ReadDescriptorString(data, descriptor->VendorIdOffset);
    drive.product = ReadDescriptorString(data, descriptor->ProductIdOffset);
    drive.firmware = ReadDescriptorString(data, descriptor->ProductRevisionOffset);
    drive.bus = BusName(descriptor->BusType);
}

struct SptiPacket final {
    SCSI_PASS_THROUGH_DIRECT request{};
    ULONG alignment = 0;
    std::array<UCHAR, kSenseLength> sense{};
};

[[nodiscard]] bool SendScsiDataIn(
    const HANDLE handle,
    const std::uint8_t* cdb,
    const std::size_t cdbLength,
    std::uint8_t* output,
    const std::size_t outputLength,
    const ULONG timeoutSeconds = 10) {
    if (cdbLength == 0 || cdbLength > 16 || output == nullptr || outputLength == 0) {
        return false;
    }

    SptiPacket packet{};
    packet.request.Length = sizeof(SCSI_PASS_THROUGH_DIRECT);
    packet.request.CdbLength = static_cast<UCHAR>(cdbLength);
    packet.request.SenseInfoLength = static_cast<UCHAR>(packet.sense.size());
    packet.request.DataIn = SCSI_IOCTL_DATA_IN;
    packet.request.DataTransferLength = static_cast<ULONG>(outputLength);
    packet.request.TimeOutValue = timeoutSeconds;
    packet.request.DataBuffer = output;
    packet.request.SenseInfoOffset = offsetof(SptiPacket, sense);
    std::memcpy(packet.request.Cdb, cdb, cdbLength);

    DWORD bytesReturned = 0;
    const BOOL success = DeviceIoControl(
        handle,
        IOCTL_SCSI_PASS_THROUGH_DIRECT,
        &packet,
        sizeof(packet),
        &packet,
        sizeof(packet),
        &bytesReturned,
        nullptr);
    return success != FALSE && packet.request.ScsiStatus == 0;
}

void AddWriteSpeed(
    OpticalDrive& drive,
    const std::uint32_t kilobytesPerSecond,
    const bool exact,
    std::string rotation) {
    if (kilobytesPerSecond < 100 || kilobytesPerSecond > 100000) {
        return;
    }
    const auto duplicate = std::find_if(
        drive.writeSpeeds.begin(),
        drive.writeSpeeds.end(),
        [kilobytesPerSecond](const WriteSpeed& speed) {
            return speed.kilobytesPerSecond == kilobytesPerSecond;
        });
    if (duplicate != drive.writeSpeeds.end()) {
        duplicate->exactForWholeMedia = duplicate->exactForWholeMedia || exact;
        return;
    }

    WriteSpeed speed;
    speed.kilobytesPerSecond = kilobytesPerSecond;
    speed.cdMultiplier = static_cast<float>(kilobytesPerSecond) /
        kCdOneXKilobytesPerSecond;
    speed.exactForWholeMedia = exact;
    speed.rotation = std::move(rotation);
    drive.writeSpeeds.push_back(std::move(speed));
}

[[nodiscard]] std::string RotationName(const unsigned value) {
    switch (value) {
    case 0: return "CLV/default";
    case 1: return "CAV";
    default: return "other";
    }
}

[[nodiscard]] bool QueryWindowsWriteSpeeds(
    const HANDLE handle,
    OpticalDrive& drive) {
    CDROM_WRITE_SPEED_REQUEST request{};
    request.RequestType = CdromWriteSpeedRequest;

    std::array<std::uint8_t, 64U * 1024U> output{};
    DWORD bytesReturned = 0;
    if (!DeviceIoControl(
            handle,
            IOCTL_CDROM_GET_PERFORMANCE,
            &request,
            sizeof(request),
            output.data(),
            static_cast<DWORD>(output.size()),
            &bytesReturned,
            nullptr)) {
        return false;
    }

    constexpr std::size_t headerSize = offsetof(CDROM_PERFORMANCE_HEADER, Data);
    if (bytesReturned < headerSize) {
        return false;
    }

    const auto* header =
        reinterpret_cast<const CDROM_PERFORMANCE_HEADER*>(output.data());
    const std::size_t reportedSize =
        static_cast<std::size_t>(ReadBigEndian32(header->DataLength)) + 4U;
    const std::size_t usableSize = std::min<std::size_t>(
        bytesReturned,
        std::min<std::size_t>(reportedSize, output.size()));
    if (usableSize < headerSize) {
        return false;
    }

    const std::size_t before = drive.writeSpeeds.size();
    const std::size_t descriptorCount =
        (usableSize - headerSize) / sizeof(CDROM_WRITE_SPEED_DESCRIPTOR);
    const auto* descriptors = reinterpret_cast<const CDROM_WRITE_SPEED_DESCRIPTOR*>(
        output.data() + headerSize);
    for (std::size_t index = 0; index < descriptorCount; ++index) {
        const auto& descriptor = descriptors[index];
        AddWriteSpeed(
            drive,
            ReadBigEndian32(descriptor.WriteSpeed),
            descriptor.Exact != 0,
            RotationName(static_cast<unsigned>(descriptor.WriteRotationControl)));
    }
    return drive.writeSpeeds.size() > before;
}

[[nodiscard]] bool QueryRawWriteSpeeds(
    const HANDLE handle,
    OpticalDrive& drive) {
    constexpr std::size_t descriptorCount = 100;
    std::array<std::uint8_t, 8 + descriptorCount * 16> output{};
    std::array<std::uint8_t, 12> cdb{};
    cdb[0] = 0xAC; // GET PERFORMANCE
    cdb[9] = static_cast<std::uint8_t>(descriptorCount);
    cdb[10] = 0x03; // Write-speed descriptors
    if (!SendScsiDataIn(handle, cdb.data(), cdb.size(), output.data(), output.size())) {
        return false;
    }

    const std::size_t reportedSize =
        static_cast<std::size_t>(ReadBigEndian32(output.data())) + 4U;
    const std::size_t usableSize = std::min(reportedSize, output.size());
    if (usableSize < 8) {
        return false;
    }

    const std::size_t before = drive.writeSpeeds.size();
    const std::size_t count = std::min(
        descriptorCount,
        (usableSize - 8U) / 16U);
    for (std::size_t index = 0; index < count; ++index) {
        const std::uint8_t* descriptor = output.data() + 8U + index * 16U;
        const std::uint8_t flags = descriptor[0];
        AddWriteSpeed(
            drive,
            ReadBigEndian32(descriptor + 12),
            (flags & 0x02U) != 0,
            RotationName((flags >> 3U) & 0x03U));
    }
    return drive.writeSpeeds.size() > before;
}

[[nodiscard]] bool QueryModePageCapabilities(
    const HANDLE handle,
    OpticalDrive& drive) {
    std::array<std::uint8_t, 512> output{};
    std::array<std::uint8_t, 10> cdb{};
    cdb[0] = 0x5A; // MODE SENSE(10)
    cdb[1] = 0x08; // Disable block descriptors
    cdb[2] = 0x2A; // CD/DVD capabilities and mechanical status page
    cdb[7] = static_cast<std::uint8_t>((output.size() >> 8U) & 0xFFU);
    cdb[8] = static_cast<std::uint8_t>(output.size() & 0xFFU);
    if (!SendScsiDataIn(handle, cdb.data(), cdb.size(), output.data(), output.size())) {
        return false;
    }

    const std::size_t reportedSize =
        std::min<std::size_t>(ReadBigEndian16(output.data()) + 2U, output.size());
    if (reportedSize < 10) {
        return false;
    }
    const std::size_t blockDescriptorLength = ReadBigEndian16(output.data() + 6);
    const std::size_t pageOffset = 8U + blockDescriptorLength;
    if (pageOffset + 2U > reportedSize) {
        return false;
    }

    const std::uint8_t* page = output.data() + pageOffset;
    if ((page[0] & 0x3FU) != 0x2AU) {
        return false;
    }
    const std::size_t pageSize = std::min<std::size_t>(
        static_cast<std::size_t>(page[1]) + 2U,
        reportedSize - pageOffset);
    if (pageSize < 4) {
        return false;
    }

    drive.cdWriteCapabilityKnown = true;
    drive.canWriteCdR = (page[3] & 0x01U) != 0;

    const std::size_t before = drive.writeSpeeds.size();
    if (pageSize >= 32) {
        const std::size_t declaredCount = ReadBigEndian16(page + 30);
        const std::size_t availableCount = (pageSize - 32U) / 4U;
        const std::size_t count = std::min(declaredCount, availableCount);
        for (std::size_t index = 0; index < count; ++index) {
            const std::uint8_t* descriptor = page + 32U + index * 4U;
            AddWriteSpeed(
                drive,
                ReadBigEndian16(descriptor + 2),
                false,
                RotationName(descriptor[1] & 0x03U));
        }
    }

    if (drive.writeSpeeds.size() == before && pageSize >= 22) {
        AddWriteSpeed(drive, ReadBigEndian16(page + 18), false, "drive maximum");
        const std::uint32_t current = pageSize >= 30
            ? ReadBigEndian16(page + 28)
            : ReadBigEndian16(page + 20);
        AddWriteSpeed(drive, current, false, "current/default");
    }
    return drive.writeSpeeds.size() > before;
}

[[nodiscard]] std::string ProfileName(const std::uint16_t profile) {
    switch (profile) {
    case 0x0008: return "CD-ROM";
    case 0x0009: return "CD-R";
    case 0x000A: return "CD-RW";
    case 0x0010: return "DVD-ROM";
    case 0x0011: return "DVD-R sequential";
    case 0x0012: return "DVD-RAM";
    case 0x0013: return "DVD-RW restricted overwrite";
    case 0x0014: return "DVD-RW sequential";
    case 0x0015: return "DVD-R DL sequential";
    case 0x0016: return "DVD-R DL layer jump";
    case 0x001A: return "DVD+RW";
    case 0x001B: return "DVD+R";
    case 0x002A: return "DVD+RW DL";
    case 0x002B: return "DVD+R DL";
    case 0x0040: return "BD-ROM";
    case 0x0041: return "BD-R sequential";
    case 0x0042: return "BD-R random";
    case 0x0043: return "BD-RE";
    default: return {};
    }
}

void QueryCurrentProfile(const HANDLE handle, OpticalDrive& drive) {
    std::array<std::uint8_t, 8> output{};
    std::array<std::uint8_t, 10> cdb{};
    cdb[0] = 0x46; // GET CONFIGURATION
    cdb[7] = 0;
    cdb[8] = static_cast<std::uint8_t>(output.size());
    if (SendScsiDataIn(handle, cdb.data(), cdb.size(), output.data(), output.size())) {
        drive.currentProfile = ReadBigEndian16(output.data() + 6);
    }
}

void QueryBlankMedia(const HANDLE handle, OpticalDrive& drive) {
    std::array<std::uint8_t, 32> output{};
    std::array<std::uint8_t, 10> cdb{};
    cdb[0] = 0x51; // READ DISC INFORMATION
    cdb[7] = 0;
    cdb[8] = static_cast<std::uint8_t>(output.size());
    if (!SendScsiDataIn(handle, cdb.data(), cdb.size(), output.data(), output.size())) {
        return;
    }

    drive.blankMediaKnown = true;
    drive.blankMedia = (output[2] & 0x03U) == 0;
}

void QueryBufferCapacity(const HANDLE handle, OpticalDrive& drive) {
    std::array<std::uint8_t, 12> output{};
    std::array<std::uint8_t, 10> cdb{};
    cdb[0] = 0x5C; // READ BUFFER CAPACITY
    cdb[7] = 0;
    cdb[8] = static_cast<std::uint8_t>(output.size());

    if (!SendScsiDataIn(
            handle,
            cdb.data(),
            cdb.size(),
            output.data(),
            output.size())) {
        return;
    }

    const std::uint32_t capacity = ReadBigEndian32(output.data() + 4);
    const std::uint32_t available = ReadBigEndian32(output.data() + 8);
    if (capacity == 0) {
        return;
    }

    drive.driveBufferCapacityKnown = true;
    drive.driveBufferCapacityBytes = capacity;
    drive.driveBufferAvailableBytes = std::min(available, capacity);
}

void QueryMediaAndWriteSpeeds(const HANDLE handle, OpticalDrive& drive) {
    DWORD bytesReturned = 0;
    DWORD changeCount = 0;
    drive.mediaPresent = DeviceIoControl(
        handle,
        IOCTL_STORAGE_CHECK_VERIFY,
        nullptr,
        0,
        &changeCount,
        sizeof(changeCount),
        &bytesReturned,
        nullptr) != FALSE;

    QueryCurrentProfile(handle, drive);
    if (drive.mediaPresent) {
        QueryBlankMedia(handle, drive);
    }

    const bool windowsSpeeds = QueryWindowsWriteSpeeds(handle, drive);
    const bool rawSpeeds = QueryRawWriteSpeeds(handle, drive);
    const bool modeSpeeds = QueryModePageCapabilities(handle, drive);
    QueryBufferCapacity(handle, drive);

    std::sort(
        drive.writeSpeeds.begin(),
        drive.writeSpeeds.end(),
        [](const WriteSpeed& left, const WriteSpeed& right) {
            return left.kilobytesPerSecond < right.kilobytesPerSecond;
        });

    if (rawSpeeds) {
        drive.speedQueryMessage = drive.mediaPresent
            ? "Direct firmware/MMC speeds for the inserted media."
            : "Direct firmware/MMC drive speeds; insert writable media for media-specific values.";
    } else if (windowsSpeeds) {
        drive.speedQueryMessage = drive.mediaPresent
            ? "Windows MMC speeds for the inserted media."
            : "Windows MMC drive speeds; insert writable media for media-specific values.";
    } else if (modeSpeeds) {
        drive.speedQueryMessage =
            "Drive capability speeds (firmware did not publish media descriptors).";
    } else if (drive.mediaPresent) {
        drive.speedQueryMessage =
            "No discrete speeds reported; Automatic will let the recording backend negotiate.";
    } else {
        drive.speedQueryMessage = "Insert blank writable media, then press Refresh.";
    }

    if (!drive.mediaPresent) {
        drive.mediaDescription = "No disc / not ready";
        return;
    }

    const std::string profile = ProfileName(drive.currentProfile);
    if (!profile.empty()) {
        drive.mediaDescription = profile;
    } else {
        drive.mediaDescription = "Disc present";
    }
    if (drive.blankMediaKnown) {
        drive.mediaDescription += drive.blankMedia ? " - blank" : " - not blank";
    } else {
        drive.mediaDescription += " - blank state unconfirmed";
    }
}

void QueryCdrecordAddress(const HANDLE handle, OpticalDrive& drive) {
    SCSI_ADDRESS address{};
    address.Length = sizeof(address);
    DWORD bytesReturned = 0;
    if (DeviceIoControl(
            handle,
            IOCTL_SCSI_GET_ADDRESS,
            nullptr,
            0,
            &address,
            sizeof(address),
            &bytesReturned,
            nullptr)) {
        drive.scsiAddressValid = true;
        drive.scsiBusKey = static_cast<std::uint16_t>(
            ((address.PortNumber & 0xFFU) << 8U) | address.PathId);
        drive.scsiTarget = address.TargetId;
        drive.scsiLun = address.Lun;
        return;
    }

    // cdrtools uses the drive-letter index as a synthetic adapter for USB and
    // FireWire bridges that return ERROR_NOT_SUPPORTED for this IOCTL.
    if (GetLastError() == ERROR_NOT_SUPPORTED && !drive.rootPath.empty()) {
        const unsigned driveIndex =
            static_cast<unsigned>(std::towupper(drive.rootPath.front()) - L'A');
        drive.scsiAddressValid = true;
        drive.scsiBusKey = static_cast<std::uint16_t>((driveIndex + 64U) << 8U);
        drive.scsiTarget = 0;
        drive.scsiLun = 0;
    }
}

[[nodiscard]] std::vector<std::uint16_t> EnumerateCdrtoolsAdapterKeys() {
    std::vector<std::uint16_t> keys;
    std::array<std::uint8_t, 64U * 1024U> inquiry{};

    for (unsigned port = 0; port <= 255; ++port) {
        const std::wstring path = L"\\\\.\\SCSI" + std::to_wstring(port) + L":";
        UniqueHandle handle(CreateFileW(
            path.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr));
        if (!handle.IsValid()) {
            break;
        }

        DWORD bytesReturned = 0;
        if (!DeviceIoControl(
                handle.Get(),
                IOCTL_SCSI_GET_INQUIRY_DATA,
                nullptr,
                0,
                inquiry.data(),
                static_cast<DWORD>(inquiry.size()),
                &bytesReturned,
                nullptr) ||
            bytesReturned < sizeof(SCSI_ADAPTER_BUS_INFO)) {
            continue;
        }

        const auto* adapter =
            reinterpret_cast<const SCSI_ADAPTER_BUS_INFO*>(inquiry.data());
        for (unsigned busPath = 0;
             busPath < adapter->NumberOfBuses;
             ++busPath) {
            keys.push_back(
                static_cast<std::uint16_t>((port << 8U) | busPath));
        }
    }
    return keys;
}

struct CdrecordScanEntry final {
    std::string address;
    std::string vendor;
    std::string product;
    std::string revision;
};

[[nodiscard]] std::string NormalizeInquiryField(std::string value) {
    value = TrimAscii(std::move(value));
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](const unsigned char c) {
            return static_cast<char>(std::toupper(c));
        });
    return value;
}

[[nodiscard]] bool RunProcessCapture(
    const std::wstring& executable,
    const std::wstring& arguments,
    std::string& output) {
    SECURITY_ATTRIBUTES security{};
    security.nLength = sizeof(security);
    security.bInheritHandle = TRUE;

    HANDLE readPipeRaw = nullptr;
    HANDLE writePipeRaw = nullptr;
    if (!CreatePipe(&readPipeRaw, &writePipeRaw, &security, 0)) {
        return false;
    }

    UniqueHandle readPipe(readPipeRaw);
    UniqueHandle writePipe(writePipeRaw);
    SetHandleInformation(readPipe.Get(), HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = writePipe.Get();
    startup.hStdError = writePipe.Get();
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION process{};

    std::wstring commandLine = L"\"" + executable + L"\" " + arguments;
    std::vector<wchar_t> mutableCommand(
        commandLine.begin(), commandLine.end());
    mutableCommand.push_back(L'\0');

    const BOOL started = CreateProcessW(
        nullptr,
        mutableCommand.data(),
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW,
        nullptr,
        nullptr,
        &startup,
        &process);

    if (!started) {
        return false;
    }

    UniqueHandle processHandle(process.hProcess);
    UniqueHandle threadHandle(process.hThread);

    // The parent must close its write end or ReadFile will never see EOF.
    writePipe = UniqueHandle();

    output.clear();
    std::array<char, 4096> buffer{};
    DWORD read = 0;
    while (ReadFile(
               readPipe.Get(),
               buffer.data(),
               static_cast<DWORD>(buffer.size()),
               &read,
               nullptr) &&
           read > 0) {
        output.append(buffer.data(), read);
    }

    WaitForSingleObject(processHandle.Get(), INFINITE);

    DWORD exitCode = 1;
    if (!GetExitCodeProcess(processHandle.Get(), &exitCode)) {
        return false;
    }
    return exitCode == 0;
}

[[nodiscard]] std::vector<CdrecordScanEntry> ScanCdrecordDevices() {
    std::vector<CdrecordScanEntry> entries;
    const EmbeddedToolPaths& tools = GetEmbeddedToolPaths();
    if (!tools.Ready()) {
        return entries;
    }
    const std::filesystem::path& cdrecord = tools.retrobeam;

    std::string output;
    if (!RunProcessCapture(cdrecord.wstring(), L"-scanbus", output)) {
        return entries;
    }

    // Example:
    // 1,0,0   100) 'ASUS    ' 'SDRW-08U9M-U    ' 'B201' Removable CD-ROM
    static const std::regex linePattern(
        R"(^\s*(\d+,\d+,\d+)\s+\d+\)\s+'([^']*)'\s+'([^']*)'\s+'([^']*)')",
        std::regex::icase);

    std::istringstream lines(output);
    std::string line;
    while (std::getline(lines, line)) {
        std::smatch match;
        if (!std::regex_search(line, match, linePattern)) {
            continue;
        }

        CdrecordScanEntry entry;
        entry.address = match[1].str();
        entry.vendor = NormalizeInquiryField(match[2].str());
        entry.product = NormalizeInquiryField(match[3].str());
        entry.revision = NormalizeInquiryField(match[4].str());
        entries.push_back(std::move(entry));
    }

    return entries;
}
void AssignCdrecordAddresses(std::vector<OpticalDrive>& drives) {
    const std::vector<CdrecordScanEntry> scanned = ScanCdrecordDevices();
    std::vector<bool> used(scanned.size(), false);

    for (OpticalDrive& drive : drives) {
        drive.cdrecordDevice.clear();

        const std::string vendor = NormalizeInquiryField(drive.vendor);
        const std::string product = NormalizeInquiryField(drive.product);
        const std::string revision = NormalizeInquiryField(drive.firmware);

        // First try an exact vendor/product/revision match.
        for (std::size_t index = 0; index < scanned.size(); ++index) {
            if (used[index]) {
                continue;
            }
            const CdrecordScanEntry& candidate = scanned[index];
            if (candidate.vendor == vendor &&
                candidate.product == product &&
                candidate.revision == revision) {
                drive.cdrecordDevice = candidate.address;
                used[index] = true;
                break;
            }
        }

        // Some USB bridges do not expose the same revision through both APIs.
        // Fall back to vendor/product if the exact match failed.
        if (drive.cdrecordDevice.empty()) {
            for (std::size_t index = 0; index < scanned.size(); ++index) {
                if (used[index]) {
                    continue;
                }
                const CdrecordScanEntry& candidate = scanned[index];
                if (candidate.vendor == vendor &&
                    candidate.product == product) {
                    drive.cdrecordDevice = candidate.address;
                    used[index] = true;
                    break;
                }
            }
        }
    }
}


void QueryRetroBeamCapabilities(OpticalDrive& drive) {
    drive.retrobeamCapabilitiesKnown = false;
    drive.burnFreeSupported = false;
    drive.forceSpeedSupported = false;
    drive.realTimeStreamingKnown = false;
    drive.realTimeStreamingCurrent = false;
    drive.realTimeStreamingPersistent = false;
    drive.streamRecordingSupported = false;
    drive.getPerformanceWriteSpeedSupported = false;
    drive.modePage2AWriteSpeedSupported = false;
    drive.setCdSpeedSupported = false;
    drive.readBufferCapacitySupported = false;
    drive.opcDescriptorCountKnown = false;
    drive.opcDescriptorCount = 0;
    drive.advancedCapabilityMessage.clear();

    if (drive.cdrecordDevice.empty()) {
        drive.advancedCapabilityMessage =
            "RetroBeam could not map this drive to a SCSI address.";
        return;
    }

    const EmbeddedToolPaths& tools = GetEmbeddedToolPaths();
    if (!tools.Ready() || tools.retrobeam.empty()) {
        drive.advancedCapabilityMessage =
            "RetroBeam capability query is unavailable.";
        return;
    }

    const std::wstring devArg =
        L"dev=" + std::wstring(
            drive.cdrecordDevice.begin(),
            drive.cdrecordDevice.end());

    std::string prcap;
    (void)RunProcessCapture(
        tools.retrobeam.wstring(),
        devArg + L" -prcap",
        prcap);

    // RetroBeam inherits cdrecord's driver-capability flag line. These names
    // are emitted only when the selected driver advertises the capability.
    drive.burnFreeSupported =
        prcap.find("BURNFREE") != std::string::npos;
    drive.forceSpeedSupported =
        prcap.find("FORCESPEED") != std::string::npos;

    std::string mediaInfo;
    (void)RunProcessCapture(
        tools.retrobeam.wstring(),
        devArg + L" -media-info",
        mediaInfo);

    static const std::regex realtimePattern(
        R"(\[RBMI\]\s+realtime_streaming=current:(\d+)\s+persistent:(\d+)\s+stream_recording:(\d+)\s+get_performance_wspd:(\d+)\s+mode_page_2a_wspd:(\d+)\s+set_cd_speed:(\d+)\s+read_buffer_capacity:(\d+))",
        std::regex::icase);
    static const std::regex opcPattern(
        R"(\[RBMI\]\s+opc_descriptors=(\d+))",
        std::regex::icase);

    std::smatch match;
    if (std::regex_search(mediaInfo, match, realtimePattern)) {
        drive.realTimeStreamingKnown = true;
        drive.realTimeStreamingCurrent = std::stoi(match[1].str()) != 0;
        drive.realTimeStreamingPersistent = std::stoi(match[2].str()) != 0;
        drive.streamRecordingSupported = std::stoi(match[3].str()) != 0;
        drive.getPerformanceWriteSpeedSupported = std::stoi(match[4].str()) != 0;
        drive.modePage2AWriteSpeedSupported = std::stoi(match[5].str()) != 0;
        drive.setCdSpeedSupported = std::stoi(match[6].str()) != 0;
        drive.readBufferCapacitySupported = std::stoi(match[7].str()) != 0;
    } else if (
        mediaInfo.find("[RBMI] realtime_streaming=unavailable") != std::string::npos ||
        mediaInfo.find("[RBMI] realtime_streaming=not_returned") != std::string::npos ||
        mediaInfo.find("[RBMI] realtime_streaming=short_response") != std::string::npos) {
        drive.realTimeStreamingKnown = true;
    }

    if (std::regex_search(mediaInfo, match, opcPattern)) {
        drive.opcDescriptorCountKnown = true;
        drive.opcDescriptorCount = static_cast<std::uint32_t>(
            std::stoul(match[1].str()));
    }

    drive.retrobeamCapabilitiesKnown =
        !prcap.empty() || !mediaInfo.empty();

    if (drive.retrobeamCapabilitiesKnown) {
        drive.advancedCapabilityMessage =
            "RetroBeam capability fingerprint loaded for this drive/media.";
    } else {
        drive.advancedCapabilityMessage =
            "RetroBeam did not return a capability fingerprint.";
    }
}

} // namespace

std::string OpticalDrive::DisplayName() const {
    std::string letter;
    if (!rootPath.empty()) {
        letter.push_back(static_cast<char>(rootPath.front()));
        letter.push_back(':');
    }

    std::string identity;
    if (!vendor.empty()) {
        identity += vendor;
    }
    if (!product.empty()) {
        if (!identity.empty()) {
            identity += ' ';
        }
        identity += product;
    }
    if (identity.empty()) {
        identity = "Optical drive";
    }
    return letter + "  " + identity;
}

std::vector<OpticalDrive> EnumerateOpticalDrives() {
    std::vector<OpticalDrive> drives;
    std::array<wchar_t, 512> paths{};
    const DWORD pathLength = GetLogicalDriveStringsW(
        static_cast<DWORD>(paths.size()), paths.data());
    if (pathLength == 0 || pathLength >= paths.size()) {
        return drives;
    }

    for (const wchar_t* path = paths.data(); *path != L'\0';
         path += std::wcslen(path) + 1) {
        if (GetDriveTypeW(path) != DRIVE_CDROM || std::wcslen(path) < 2) {
            continue;
        }

        OpticalDrive drive;
        drive.rootPath = path;
        drive.devicePath = L"\\\\.\\";
        drive.devicePath.push_back(path[0]);
        drive.devicePath.push_back(L':');

        UniqueHandle handle = OpenDrive(drive.devicePath);
        if (handle.IsValid()) {
            QueryIdentity(handle.Get(), drive);
            QueryMediaAndWriteSpeeds(handle.Get(), drive);
            QueryCdrecordAddress(handle.Get(), drive);
        } else {
            drive.speedQueryMessage =
                "Windows could not open this drive: " +
                Win32ErrorMessage(GetLastError());
            drive.mediaDescription = "Drive unavailable";
        }
        drives.push_back(std::move(drive));
    }

    std::sort(
        drives.begin(),
        drives.end(),
        [](const OpticalDrive& left, const OpticalDrive& right) {
            return left.rootPath < right.rootPath;
        });
    AssignCdrecordAddresses(drives);
    for (OpticalDrive& drive : drives) {
        QueryRetroBeamCapabilities(drive);
    }
    return drives;
}
