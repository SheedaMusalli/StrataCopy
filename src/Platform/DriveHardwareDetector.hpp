#pragma once
#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QVariantMap>
namespace strata {
enum class StorageTier { Unknown, Mechanical, Removable, Ssd, Nvme, Network };
class DriveHardwareDetector {
  public:
    static StorageTier tier(const QString &path);
    static QString describe(const QString &path);
    static int logicalProcessors();
    static quint64 memoryBytes();
    static int recommendedThreads(StorageTier tier, int processors, quint64 memoryBytes);
    static int storageThreadLimit(StorageTier tier, int processors, quint64 memory, const QVariantMap &options);
    static int transferThreads(const QStringList &sources, const QString &destination, const QVariantMap &options = {});
    static QJsonObject inventory();
    static bool saveInventory(const QString &directory);
};
} // namespace strata
