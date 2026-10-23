#include "RobocopyCommandCompiler.hpp"
#include <QDir>
#include <windows.h>
namespace strata {
QString RobocopyCommandCompiler::executable() {
    wchar_t path[MAX_PATH + 1]{};
    GetSystemDirectoryW(path, MAX_PATH);
    return QDir(QString::fromWCharArray(path)).filePath("robocopy.exe");
}
QStringList RobocopyCommandCompiler::arguments(const RobocopyInvocation &job, const RobocopyProfile &p,
                                               const QString &unicodeLog) {
    QStringList args{QDir::toNativeSeparators(job.source), QDir::toNativeSeparators(job.destination)};
    args << job.files;
    if (!job.looseFiles) {
        switch (p.mode()) {
        case TransferMode::CopyAll:
            args << "/E";
            break;
        case TransferMode::CopyNonEmpty:
            args << "/S";
            break;
        case TransferMode::RootOnly:
            args << "/LEV:1";
            break;
        case TransferMode::MoveTree:
            args << "/E" << "/MOVE";
            break;
        case TransferMode::MoveFiles:
            args << "/E" << "/MOV";
            break;
        case TransferMode::Mirror:
            args << "/MIR";
            break;
        }
    } else {
        args << "/LEV:1";
        if (p.mode() == TransferMode::MoveTree || p.mode() == TransferMode::MoveFiles)
            args << "/MOV";
    }
    switch (static_cast<CollisionPolicy>(p.number("collisionPolicy"))) {
    case CollisionPolicy::Smart:
    case CollisionPolicy::Ask:
        break;
    case CollisionPolicy::KeepNewer:
        args << "/XO";
        break;
    case CollisionPolicy::SkipExisting:
        args << "/XC" << "/XN" << "/XO";
        break;
    case CollisionPolicy::Overwrite:
        args << "/IS" << "/IT";
        break;
    }
    switch (static_cast<AccessMode>(p.number("accessMode"))) {
    case AccessMode::Standard:
        break;
    case AccessMode::Restartable:
        args << "/Z";
        break;
    case AccessMode::Backup:
        args << "/B";
        break;
    case AccessMode::RestartableBackup:
        args << "/ZB";
        break;
    }
    if (p.flag("enableMultithreading") && !p.flag("enableBandwidthLimit"))
        args << QString("/MT:%1").arg(p.number("threadCount"));
    if (p.flag("enableBandwidthLimit"))
        args << QString("/IPG:%1").arg(p.number("interPacketGapMs"));
    for (const auto &[key, flag] :
         QList<QPair<QString, QString>>{{"unbufferedIo", "/J"},
                                        {"smbCompression", "/COMPRESS"},
                                        {"disableOffload", "/NOOFFLOAD"},
                                        {"preserveDirectoryTimestamps", "/DCOPY:DAT"},
                                        {"excludeJunctionLoops", "/XJ"},
                                        {"fatTwoSecondTolerance", "/FFT"},
                                        {"copySymlinksAsLinks", "/SL"},
                                        {"fixSecurityOnExisting", "/SECFIX"},
                                        {"fixTimestampsOnExisting", "/TIMFIX"},
                                        {"waitForShare", "/TBD"},
                                        {"dryRunListOnly", "/L"},
                                        {"createZeroByteTree", "/CREATE"},
                                        {"excludeExtraLogs", "/XX"},
                                        {"excludeLonelyFiles", "/XL"}})
        if (p.flag(key))
            args << flag;
    QString attributes;
    for (const auto &[key, letter] : QList<QPair<QString, QString>>{{"copyData", "D"},
                                                                    {"copyAttributes", "A"},
                                                                    {"copyTimestamps", "T"},
                                                                    {"copySecurityAcl", "S"},
                                                                    {"copyOwner", "O"},
                                                                    {"copyAuditing", "U"}})
        if (p.flag(key))
            attributes += letter;
    args << "/COPY:" + attributes;
    if (p.flag("fastFailSafeRetries"))
        args << QString("/R:%1").arg(p.number("retryCount"))
             << QString("/W:%1").arg(p.number("retryWaitSeconds"));
    for (const auto &[key, flag] :
         QList<QPair<QString, QString>>{{"excludeFilePatterns", "/XF"}, {"excludeDirPatterns", "/XD"}}) {
        auto patterns = p.string(key).split(';', Qt::SkipEmptyParts);
        if (!patterns.isEmpty()) {
            args << flag;
            for (const auto &pattern : patterns)
                args << pattern.trimmed();
        }
    }
    for (const auto &[key, flag] : QList<QPair<QString, QString>>{{"minFileSizeBytes", "/MIN:"},
                                                                  {"maxFileSizeBytes", "/MAX:"},
                                                                  {"maxFileAge", "/MAXAGE:"},
                                                                  {"minFileAge", "/MINAGE:"}})
        if (!p.string(key).isEmpty())
            args << flag + p.string(key);
    // /UNICODE can emit mixed encoding on stdout. The supervisor tails /UNILOG instead.
    args << "/V" << "/TS" << "/FP" << "/BYTES" << "/NJH" << "/NJS" << "/NDL" << "/NP" << "/UNICODE";
    if (!unicodeLog.isEmpty())
        args << "/UNILOG:" + QDir::toNativeSeparators(unicodeLog);
    return args;
}
QString RobocopyCommandCompiler::quote(const QString &arg) {
    QString out = "\"";
    int slashes = 0;
    for (QChar c : arg) {
        if (c == '\\') {
            ++slashes;
            continue;
        }
        if (c == '"') {
            out += QString(slashes * 2 + 1, '\\');
            out += c;
            slashes = 0;
            continue;
        }
        out += QString(slashes, '\\');
        slashes = 0;
        out += c;
    }
    out += QString(slashes * 2, '\\');
    out += '"';
    return out;
}
QString RobocopyCommandCompiler::display(const RobocopyInvocation &job, const RobocopyProfile &p,
                                         const QString &unicodeLog) {
    QStringList out{quote(executable())};
    for (const auto &a : arguments(job, p, unicodeLog))
        out << quote(a);
    return out.join(' ');
}
QString RobocopyCommandCompiler::powerShell(const QList<RobocopyInvocation> &jobs, const RobocopyProfile &p) {
    auto literal = [](QString s) { return "'" + s.replace("'", "''") + "'"; };
    QString text =
        "# StrataCopy export. Robocopy exit codes 0–7 are successful.\r\n$ErrorActionPreference = 'Stop'\r\n";
    for (const auto &j : jobs) {
        QStringList args;
        for (const auto &a : arguments(j, p))
            args << literal(a);
        text += "& " + literal(executable()) + " @( " + args.join(", ") +
                " )\r\nif ($LASTEXITCODE -ge 8) { exit $LASTEXITCODE }\r\n";
    }
    return text + "exit 0\r\n";
}
} // namespace strata
