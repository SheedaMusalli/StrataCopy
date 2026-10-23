#include "DriveHardwareDetector.hpp"
#include "UniqueHandle.hpp"
#include <QDir>
#include <QSet>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QDateTime>
#include <algorithm>
#include <array>
#include <cstring>
#include <winioctl.h>
namespace strata {
StorageTier DriveHardwareDetector::tier(const QString &path) {
    if (path.isEmpty()) return StorageTier::Unknown;
    const auto native = QDir::toNativeSeparators(QFileInfo(path).absoluteFilePath());
    if (native.startsWith("\\\\")) return StorageTier::Network;
    // Resolve mounted folders as well as drive letters. Destination folders may not exist yet.
    auto existing = native;
    while (!QFileInfo::exists(existing) && QFileInfo(existing).absolutePath() != existing)
        existing = QFileInfo(existing).absolutePath();
    wchar_t root[MAX_PATH + 1]{};
    if (!GetVolumePathNameW(reinterpret_cast<LPCWSTR>(existing.utf16()), root, MAX_PATH))
        return StorageTier::Unknown;
    const auto type = GetDriveTypeW(root);
    if (type == DRIVE_REMOTE) return StorageTier::Network;
    if (type == DRIVE_REMOVABLE || type == DRIVE_CDROM) return StorageTier::Removable;
    wchar_t volumeName[MAX_PATH + 1]{};
    if (!GetVolumeNameForVolumeMountPointW(root, volumeName, MAX_PATH)) return StorageTier::Unknown;
    auto volume = QString::fromWCharArray(volumeName);
    if (volume.endsWith('\\')) volume.chop(1);
    UniqueHandle drive(CreateFileW(reinterpret_cast<LPCWSTR>(volume.utf16()), 0,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr));
    if (!drive) return StorageTier::Unknown;
    STORAGE_PROPERTY_QUERY query{};
    query.PropertyId = StorageDeviceProperty;
    query.QueryType = PropertyStandardQuery;
    alignas(STORAGE_DEVICE_DESCRIPTOR) std::array<char, 4096> buffer{};
    DWORD count = 0;
    STORAGE_BUS_TYPE bus = BusTypeUnknown;
    if (DeviceIoControl(drive.get(), IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query), buffer.data(),
        DWORD(buffer.size()), &count, nullptr) && count >= sizeof(STORAGE_DEVICE_DESCRIPTOR)) {
        STORAGE_DEVICE_DESCRIPTOR descriptor{};
        std::memcpy(&descriptor, buffer.data(), sizeof(descriptor));
        if (descriptor.Size >= sizeof(descriptor)) bus = descriptor.BusType;
    }
    if (bus == BusTypeUsb || bus == BusTypeSd || bus == BusTypeMmc) return StorageTier::Removable;
    query.PropertyId = StorageDeviceSeekPenaltyProperty;
    DEVICE_SEEK_PENALTY_DESCRIPTOR penalty{};
    const bool seekKnown = DeviceIoControl(drive.get(), IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query),
        &penalty, sizeof(penalty), &count, nullptr) && count >= sizeof(penalty) && penalty.Size >= sizeof(penalty);
    if (seekKnown && penalty.IncursSeekPenalty) return StorageTier::Mechanical;
    if (bus == BusTypeNvme) return StorageTier::Nvme;
    // A SATA/RAID bus alone does not identify solid-state media.
    if (seekKnown) return StorageTier::Ssd;
    return StorageTier::Unknown;
}
QString DriveHardwareDetector::describe(const QString &path) {
    switch(tier(path)) {
    case StorageTier::Mechanical: return "Mechanical HDD";
    case StorageTier::Removable: return "USB / removable storage";
    case StorageTier::Ssd: return "SSD / non-seeking storage";
    case StorageTier::Nvme: return "NVMe SSD";
    case StorageTier::Network: return "Network / SMB";
    default: return "Unknown storage (conservative settings)";
    }
}
int DriveHardwareDetector::logicalProcessors() {
    return std::max(1, int(GetActiveProcessorCount(ALL_PROCESSOR_GROUPS)));
}
quint64 DriveHardwareDetector::memoryBytes() {
    MEMORYSTATUSEX memory{}; memory.dwLength = sizeof(memory);
    return GlobalMemoryStatusEx(&memory) ? memory.ullTotalPhys : 0;
}
int DriveHardwareDetector::recommendedThreads(StorageTier tier, int processors, quint64 memory) {
    processors = std::clamp(processors, 1, 1024);
    int threads = 4;
    if (tier == StorageTier::Nvme) threads = std::clamp(processors * 2, 8, 32);
    else if (tier == StorageTier::Ssd || tier == StorageTier::Network) threads = std::clamp(processors, 4, 8);
    // CPU and RAM guards apply even when a fast drive is attached to a small system.
    threads = std::min(threads, std::max(2, processors * 2));
    if (!memory || memory <= 4ULL * 1024 * 1024 * 1024) threads = std::min(threads, 4);
    else if (memory <= 8ULL * 1024 * 1024 * 1024) threads = std::min(threads, 8);
    return threads;
}
int DriveHardwareDetector::storageThreadLimit(StorageTier storage, int processors, quint64 memory, const QVariantMap &options) {
    QString key;
    switch(storage) {
    case StorageTier::Nvme: key="nvmeThreads";break;
    case StorageTier::Ssd: key="ssdThreads";break;
    case StorageTier::Mechanical: key="hddThreads";break;
    case StorageTier::Removable: key="usbThreads";break;
    case StorageTier::Network: key="networkThreads";break;
    default:key="unknownThreads";break;
    }
    const int chosen=options.value(key,0).toInt();
    return chosen>=1 && chosen<=128 ? chosen : recommendedThreads(storage,processors,memory);
}
int DriveHardwareDetector::transferThreads(const QStringList &sources, const QString &destination, const QVariantMap &options) {
    const auto cpu = logicalProcessors(); const auto ram = memoryBytes();
    int count = storageThreadLimit(tier(destination), cpu, ram, options);
    // Unique source volumes avoid reopening a device for every loose file.
    QSet<QString> seen;
    for (const auto &source : sources) {
        const auto native = QDir::toNativeSeparators(source);
        wchar_t root[MAX_PATH + 1]{};
        const auto key = GetVolumePathNameW(reinterpret_cast<LPCWSTR>(native.utf16()), root, MAX_PATH)
            ? QString::fromWCharArray(root).toCaseFolded() : native.left(3).toCaseFolded();
        if (seen.contains(key)) continue;
        seen.insert(key);
        count = std::min(count, storageThreadLimit(tier(source), cpu, ram, options));
    }
    return count;
}
QJsonObject DriveHardwareDetector::inventory() {
    QJsonArray drives;
    for (const auto &drive : QDir::drives()) {
        const auto root = QDir::toNativeSeparators(drive.absoluteFilePath());
        const auto type = GetDriveTypeW(reinterpret_cast<LPCWSTR>(root.utf16()));
        if (type == DRIVE_FIXED) drives.append(QJsonObject{{"root", drive.absoluteFilePath()},
            {"storage", describe(drive.absoluteFilePath())}});
    }
    return {{"logicalProcessors", logicalProcessors()}, {"physicalMemoryBytes", double(memoryBytes())},
        {"drives", drives}, {"detectedUtc", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
        {"defaultProfile", "Auto"}};
}
bool DriveHardwareDetector::saveInventory(const QString &directory) {
    if (!QDir().mkpath(directory)) return false;
    QSaveFile file(directory + "/hardware.json");
    const auto bytes = QJsonDocument(inventory()).toJson();
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit();
}
} // namespace strata
