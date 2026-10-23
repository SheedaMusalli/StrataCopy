#include "MultiSourceBatcher.hpp"
#include <QDir>
#include <QFileInfo>
#include <QMap>
#include <QSet>
namespace strata {
QString MultiSourceBatcher::normalized(const QString &path) {
    QFileInfo f(path);
    QString p = f.canonicalFilePath();
    if (p.isEmpty()) {
        QString ancestor = f.absoluteFilePath();
        QStringList tail;
        while (!QFileInfo::exists(ancestor)) {
            const QFileInfo current(ancestor);
            const auto leaf = current.fileName();
            const auto parent = current.absolutePath();
            if (leaf.isEmpty() || parent == ancestor)
                break;
            tail.prepend(leaf);
            ancestor = parent;
        }
        p = QFileInfo(ancestor).canonicalFilePath();
        for (const auto &part : tail)
            p = QDir(p).filePath(part);
        if (p.isEmpty())
            p = f.absoluteFilePath();
    }
    p = QDir::fromNativeSeparators(QDir::cleanPath(p));
    while (p.endsWith('/') && p.size() > 3)
        p.chop(1);
    return p;
}
bool MultiSourceBatcher::inside(const QString &path, const QString &parent) {
    QString a = normalized(path), b = normalized(parent);
    return a.compare(b, Qt::CaseInsensitive) == 0 ||
           a.startsWith(b.endsWith('/') ? b : b + '/', Qt::CaseInsensitive);
}
BatchPlan MultiSourceBatcher::build(const QStringList &paths, const QString &target,
                                    const RobocopyProfile &profile) {
    BatchPlan plan;
    plan.errors = profile.validate();
    if (paths.isEmpty())
        plan.errors << "Add at least one source file or folder.";
    if (target.trimmed().isEmpty()) {
        plan.errors << "Choose a destination folder.";
        return plan;
    }
    QFileInfo targetInfo(target);
    if (targetInfo.exists() && !targetInfo.isDir())
        plan.errors << "Destination must be a directory.";
    if (target.contains('"') || target.contains('\n') || target.contains('\r'))
        plan.errors << "Invalid destination path.";
    QMap<QString, int> loose;
    QSet<QString> seen, destinations;
    QStringList directories;
    for (const auto &p : paths)
        if (QFileInfo(p).isDir())
            directories << normalized(p);
    for (const auto &path : paths) {
        QFileInfo info(path);
        const QString source = normalized(path), key = source.toCaseFolded();
        if (seen.contains(key))
            continue;
        seen.insert(key);
        if (!info.exists()) {
            plan.errors << "Source does not exist: " + path;
            continue;
        }
        if (info.isSymLink()) {
            plan.errors << "Select the target of the top-level source link: " + path;
            continue;
        }
        if (source.contains('"') || source.contains('\n') || source.contains('\r')) {
            plan.errors << "Invalid source path.";
            continue;
        }
        bool nested = false;
        for (const auto &d : directories)
            if (profile.mode() != TransferMode::RootOnly && d.compare(source, Qt::CaseInsensitive) != 0 &&
                inside(source, d))
                nested = true;
        if (nested)
            continue;
        if (info.isDir()) {
            if (info.fileName().isEmpty()) {
                plan.errors << "Select a folder within the drive rather than a drive root.";
                continue;
            }
            const auto destination = QDir(normalized(target)).filePath(info.fileName());
            if (inside(target, source) || inside(source, destination) || inside(destination, source))
                plan.errors << "Source and destination trees must not overlap: " + source;
            if (destinations.contains(destination.toCaseFolded()))
                plan.errors << "Two sources have the same destination folder: " + info.fileName();
            destinations.insert(destination.toCaseFolded());
            plan.jobs.append({source, destination, {}, false});
        } else {
            const auto destFile = QDir(normalized(target)).filePath(info.fileName());
            if (normalized(destFile).compare(source, Qt::CaseInsensitive) == 0)
                plan.errors << "Source file is already in the destination: " + source;
            if (destinations.contains(destFile.toCaseFolded()))
                plan.errors << "Two sources have the same destination name: " + info.fileName();
            destinations.insert(destFile.toCaseFolded());
            if (profile.mode() == TransferMode::Mirror)
                plan.errors << "Mirror requires folder sources; loose-file mirroring could purge unrelated "
                               "destination data.";
            const auto parent = info.absolutePath().toCaseFolded();
            if (!loose.contains(parent)) {
                loose[parent] = int(plan.jobs.size());
                plan.jobs.append({info.absolutePath(), normalized(target), {}, true});
            }
            plan.jobs[loose[parent]].files << info.fileName();
        }
    }
    // A loose file must not map into a folder also being mirrored in this batch.
    for (const auto &a : plan.jobs)
        for (const auto &b : plan.jobs)
            if (a.looseFiles && !b.looseFiles)
                for (const auto &name : a.files)
                    if (inside(QDir(a.destination).filePath(name), b.destination))
                        plan.errors << "Destination mappings overlap.";
    return plan;
}
} // namespace strata
