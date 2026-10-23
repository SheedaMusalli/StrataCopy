#pragma once
#include "RobocopyProfile.hpp"
#include <QList>
namespace strata {
struct RobocopyInvocation {
    QString source, destination;
    QStringList files;
    bool looseFiles = false;
};
struct BatchPlan {
    QList<RobocopyInvocation> jobs;
    QStringList errors;
};
class MultiSourceBatcher {
  public:
    static BatchPlan build(const QStringList &paths, const QString &target, const RobocopyProfile &profile);
    static QString normalized(const QString &path);
    static bool inside(const QString &path, const QString &parent);
};
} // namespace strata
