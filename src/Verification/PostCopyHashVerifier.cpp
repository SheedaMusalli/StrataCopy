#include "PostCopyHashVerifier.hpp"
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <blake3.h>
#include <memory>
#include <xxhash.h>
namespace strata {
QByteArray PostCopyHashVerifier::hashFile(const QString &path, int algorithm, const std::atomic_bool &cancel,
                                          QString &error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        error = file.errorString();
        return {};
    }
    QCryptographicHash sha(QCryptographicHash::Sha256);
    std::unique_ptr<XXH3_state_t, decltype(&XXH3_freeState)> xxh(XXH3_createState(), XXH3_freeState);
    if (!xxh) {
        error = "Cannot allocate hash state.";
        return {};
    }
    XXH3_64bits_reset(xxh.get());
    blake3_hasher b3;
    blake3_hasher_init(&b3);
    QByteArray buffer(1024 * 1024, Qt::Uninitialized);
    while (!file.atEnd()) {
        if (cancel.load()) {
            error = "Cancelled";
            return {};
        }
        const auto n = file.read(buffer.data(), buffer.size());
        if (n < 0) {
            error = file.errorString();
            return {};
        }
        if (n == 0)
            break;
        if (algorithm == 0)
            XXH3_64bits_update(xxh.get(), buffer.constData(), size_t(n));
        else if (algorithm == 1)
            blake3_hasher_update(&b3, buffer.constData(), size_t(n));
        else
            sha.addData(QByteArrayView(buffer.constData(), n));
    }
    if (algorithm == 0)
        return QByteArray::number(XXH3_64bits_digest(xxh.get()), 16).rightJustified(16, '0');
    if (algorithm == 1) {
        uint8_t output[BLAKE3_OUT_LEN];
        blake3_hasher_finalize(&b3, output, sizeof(output));
        return QByteArray(reinterpret_cast<char *>(output), sizeof(output)).toHex();
    }
    return sha.result().toHex();
}
bool PostCopyHashVerifier::verifyFile(const QString &source, const QString &dest, quint64 size,
                                      const RobocopyProfile &p, const std::atomic_bool &cancel,
                                      const std::function<void(FileEntry)> &sink) {
    const QFileInfo before(source), target(dest);
    QString error;
    // Verification is a readback comparison of eligible source files, including unchanged ones.
    auto left = hashFile(source, p.number("hashAlgorithm"), cancel, error);
    auto right = error.isEmpty() ? hashFile(dest, p.number("hashAlgorithm"), cancel, error) : QByteArray();
    const QFileInfo after(source);
    bool ok = error.isEmpty() && left == right && target.size() == qint64(size) &&
              before.size() == after.size() && before.lastModified() == after.lastModified();
    if (cancel.load())
        return false;
    FileEntry entry{dest, ok ? "Hash match" : (error.isEmpty() ? "Hash mismatch or source changed" : error),
                    ok ? "Verified" : "Failed", QString::fromLatin1(right), size};
    if (ok && p.flag("writeSidecars")) {
        const auto extension = QStringList{".xxh3", ".blake3", ".sha256"}[p.number("hashAlgorithm")];
        const auto sidecarPath = dest + extension;
        if (QFileInfo::exists(sidecarPath)) {
            ok = false;
            entry.state = "Failed";
            entry.classification = "Sidecar already exists; preserved";
        } else {
            QSaveFile sidecar(sidecarPath);
            const auto contents = right + "  " + QFileInfo(dest).fileName().toUtf8() + "\n";
            if (!sidecar.open(QIODevice::WriteOnly) || sidecar.write(contents) != contents.size() ||
                !sidecar.commit()) {
                ok = false;
                entry.state = "Failed";
                entry.classification = "Cannot write hash sidecar";
            }
        }
    }
    sink(std::move(entry));
    return ok;
}
VerificationResult PostCopyHashVerifier::verify(const RobocopyInvocation &job, const RobocopyProfile &p,
                                                const std::atomic_bool &cancel,
                                                const std::function<void(FileEntry)> &sink) {
    VerificationResult result;
    const auto scan =
        FastPreScanner::scan(job, p, cancel, [&](const QString &source, const QString &dest, quint64 size) {
            const bool ok = verifyFile(source, dest, size, p, cancel, sink);
            if (cancel.load())
                return false;
            ok ? ++result.verified : ++result.failed;
            return true;
        });
    if (!scan.warnings.isEmpty()) {
        ++result.failed;
        sink({job.source, scan.warnings.join("; "), "Failed", {}, 0});
    }
    result.cancelled = cancel.load();
    return result;
}
} // namespace strata
