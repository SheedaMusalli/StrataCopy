#pragma once
#include <QStringList>
#include <QVariantMap>

namespace strata {
enum class TransferMode { CopyAll, CopyNonEmpty, RootOnly, MoveTree, MoveFiles, Mirror };
enum class CollisionPolicy { Smart, KeepNewer, SkipExisting, Overwrite, Ask };
enum class AccessMode { Standard, Restartable, Backup, RestartableBackup };

struct RobocopyProfile {
    QVariantMap values;
    RobocopyProfile();
    bool flag(const QString &key) const {
        return values.value(key).toBool();
    }
    int number(const QString &key) const {
        return values.value(key).toInt();
    }
    QString string(const QString &key) const {
        return values.value(key).toString().trimmed();
    }
    TransferMode mode() const {
        return static_cast<TransferMode>(number("transferMode"));
    }
    bool destructive() const {
        return number("transferMode") >= 3;
    }
    QStringList validate() const;
    static QVariantList schema();
    static RobocopyProfile preset(int index);
};
} // namespace strata
