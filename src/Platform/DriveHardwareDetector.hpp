#pragma once
#include <QString>
namespace strata {
class DriveHardwareDetector {
  public:
    static QString describe(const QString &path);
};
} // namespace strata
