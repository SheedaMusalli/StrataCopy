#include "DriveHardwareDetector.hpp"
#include "UniqueHandle.hpp"
#include <QDir>
#include <winioctl.h>
namespace strata {
QString DriveHardwareDetector::describe(const QString &path) {
    const auto native = QDir::toNativeSeparators(path);
    if (native.startsWith("\\\\"))
        return "Network / SMB";
    if (native.size() < 2 || native[1] != ':')
        return "Storage";
    const auto volume = "\\\\.\\" + native.left(2);
    UniqueHandle drive(CreateFileW(reinterpret_cast<LPCWSTR>(volume.utf16()), 0,
                                   FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr));
    if (!drive)
        return "Local storage";
    STORAGE_PROPERTY_QUERY query{};
    query.PropertyId = StorageDeviceProperty;
    query.QueryType = PropertyStandardQuery;
    QByteArray buffer(4096, 0);
    DWORD count = 0;
    if (DeviceIoControl(drive.get(), IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query), buffer.data(),
                        DWORD(buffer.size()), &count, nullptr)) {
        const auto *descriptor = reinterpret_cast<const STORAGE_DEVICE_DESCRIPTOR *>(buffer.constData());
        if (descriptor->BusType == BusTypeNvme)
            return "NVMe";
        if (descriptor->BusType == BusTypeUsb)
            return "USB storage";
    }
    query.PropertyId = StorageDeviceSeekPenaltyProperty;
    DEVICE_SEEK_PENALTY_DESCRIPTOR penalty{};
    if (DeviceIoControl(drive.get(), IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query), &penalty,
                        sizeof(penalty), &count, nullptr))
        return penalty.IncursSeekPenalty ? "Hard disk" : "Solid-state storage";
    return "Local storage";
}
} // namespace strata
