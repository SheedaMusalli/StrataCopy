#pragma once
#include "MultiSourceBatcher.hpp"
namespace strata {
class RobocopyCommandCompiler {
  public:
    static QString executable();
    static QStringList arguments(const RobocopyInvocation &, const RobocopyProfile &,
                                 const QString &unicodeLog = {});
    static QString quote(const QString &);
    static QString display(const RobocopyInvocation &, const RobocopyProfile &,
                           const QString &unicodeLog = {});
    static QString powerShell(const QList<RobocopyInvocation> &, const RobocopyProfile &);
};
} // namespace strata
