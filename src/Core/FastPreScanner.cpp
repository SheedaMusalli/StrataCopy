#include "FastPreScanner.hpp"
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <vector>
#include <windows.h>
namespace strata {
namespace {
struct SearchHandle {
    HANDLE h = INVALID_HANDLE_VALUE;
    ~SearchHandle() {
        if (h != INVALID_HANDLE_VALUE)
            FindClose(h);
    }
};
QString extended(QString p) {
    p = QDir::toNativeSeparators(QFileInfo(p).absoluteFilePath());
    if (p.startsWith("\\\\?\\"))
        return p;
    return p.startsWith("\\\\") ? "\\\\?\\UNC\\" + p.mid(2) : "\\\\?\\" + p;
}
bool excluded(const QString &name, const QString &path, const QStringList &patterns) {
    for (const auto &p : patterns) {
        const QRegularExpression rx(QRegularExpression::wildcardToRegularExpression(p.trimmed()),
                                    QRegularExpression::CaseInsensitiveOption);
        if (rx.match(name).hasMatch() || rx.match(QDir::toNativeSeparators(path)).hasMatch())
            return true;
    }
    return false;
}
} // namespace
ScanResult FastPreScanner::scan(const RobocopyInvocation &job, const RobocopyProfile &p,
                                const std::atomic_bool &cancel, Visitor visitor) {
    ScanResult result;
    QStringList pending{job.source};
    const auto xf = p.string("excludeFilePatterns").split(';', Qt::SkipEmptyParts),
               xd = p.string("excludeDirPatterns").split(';', Qt::SkipEmptyParts);
    while (!pending.isEmpty()) {
        if (cancel.load()) {
            result.cancelled = true;
            break;
        }
        const auto dir = pending.takeLast();
        WIN32_FIND_DATAW data{};
        SearchHandle handle;
        const auto search = extended(QDir(dir).filePath("*"));
        handle.h = FindFirstFileExW(reinterpret_cast<LPCWSTR>(search.utf16()), FindExInfoBasic, &data,
                                    FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);
        if (handle.h == INVALID_HANDLE_VALUE) {
            if (result.warnings.size() < 50)
                result.warnings << "Cannot scan " + dir + QString(" (Windows error %1)").arg(GetLastError());
            continue;
        }
        do {
            if (cancel.load()) {
                result.cancelled = true;
                break;
            }
            const auto name = QString::fromWCharArray(data.cFileName);
            if (name == "." || name == "..")
                continue;
            const auto path = QDir(dir).filePath(name);
            if (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
                if (!p.flag("excludeJunctionLoops") && result.warnings.size() < 50)
                    result.warnings << "Progress estimate excludes reparse point: " + path;
                continue;
            }
            if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                if (!job.looseFiles && p.mode() != TransferMode::RootOnly && !excluded(name, path, xd))
                    pending << path;
                continue;
            }
            if (job.looseFiles && !job.files.contains(name, Qt::CaseInsensitive))
                continue;
            if (excluded(name, path, xf))
                continue;
            const quint64 size = (quint64(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
            if (size < p.string("minFileSizeBytes").toULongLong())
                continue;
            if (!p.string("maxFileSizeBytes").isEmpty() && size > p.string("maxFileSizeBytes").toULongLong())
                continue;
            const auto relative = QDir(job.source).relativeFilePath(path),
                       dest = QDir(job.destination).filePath(relative);
            QFileInfo source(path), destination(dest);
            auto ageDate = [&](const QString &key) {
                const auto s = p.string(key);
                return s.size() == 8 ? QDate::fromString(s, "yyyyMMdd")
                                     : QDate::currentDate().addDays(-s.toInt());
            };
            if (!p.string("maxFileAge").isEmpty() && source.lastModified().date() < ageDate("maxFileAge"))
                continue;
            if (!p.string("minFileAge").isEmpty() && source.lastModified().date() > ageDate("minFileAge"))
                continue;
            if (p.flag("excludeLonelyFiles") && !destination.exists())
                continue;
            if (p.number("collisionPolicy") == 2 && destination.exists())
                continue;
            if (p.number("collisionPolicy") == 1 && destination.exists() &&
                destination.lastModified() > source.lastModified())
                continue;
            ++result.files;
            result.bytes += size;
            if (visitor && !visitor(path, dest, size)) {
                result.cancelled = cancel.load();
                return result;
            }
        } while (FindNextFileW(handle.h, &data));
        const auto err = GetLastError();
        if (err != ERROR_NO_MORE_FILES && !result.cancelled && result.warnings.size() < 50)
            result.warnings << "Directory scan incomplete: " + dir;
    }
    return result;
}
} // namespace strata
