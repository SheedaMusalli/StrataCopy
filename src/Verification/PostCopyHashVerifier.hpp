#pragma once
#include "Core/FastPreScanner.hpp"
#include "Core/StdoutPipeParser.hpp"
namespace strata {
struct VerificationResult {
    quint64 verified = 0, failed = 0;
    bool cancelled = false;
};
class PostCopyHashVerifier {
  public:
    static QByteArray hashFile(const QString &, int algorithm, const std::atomic_bool &, QString &error);
    static bool verifyFile(const QString &source, const QString &destination, quint64 size,
                           const RobocopyProfile &, const std::atomic_bool &,
                           const std::function<void(FileEntry)> &);
    static VerificationResult verify(const RobocopyInvocation &, const RobocopyProfile &,
                                     const std::atomic_bool &, const std::function<void(FileEntry)> &);
};
} // namespace strata
