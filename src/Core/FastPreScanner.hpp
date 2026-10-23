#pragma once
#include "MultiSourceBatcher.hpp"
#include <atomic>
#include <functional>
namespace strata {
struct ScanResult {
    quint64 bytes = 0, files = 0;
    QStringList warnings;
    bool cancelled = false;
};
class FastPreScanner {
  public:
    using Visitor = std::function<bool(const QString &, const QString &, quint64)>;
    static ScanResult scan(const RobocopyInvocation &, const RobocopyProfile &, const std::atomic_bool &,
                           Visitor visitor = {});
};
} // namespace strata
