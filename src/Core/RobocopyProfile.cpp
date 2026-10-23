#include "RobocopyProfile.hpp"
#include <QRegularExpression>
namespace strata {
RobocopyProfile::RobocopyProfile() {
    values = {{"transferMode", 0},
              {"collisionPolicy", 4},
              {"accessMode", 0},
              {"enableMultithreading", true},
              {"threadCount", 128},
              {"unbufferedIo", true},
              {"smbCompression", false},
              {"disableOffload", false},
              {"enableBandwidthLimit", false},
              {"interPacketGapMs", 10},
              {"copyData", true},
              {"copyAttributes", true},
              {"copyTimestamps", true},
              {"copySecurityAcl", false},
              {"copyOwner", false},
              {"copyAuditing", false},
              {"preserveDirectoryTimestamps", true},
              {"excludeJunctionLoops", true},
              {"fatTwoSecondTolerance", true},
              {"copySymlinksAsLinks", false},
              {"fixSecurityOnExisting", false},
              {"fixTimestampsOnExisting", false},
              {"fastFailSafeRetries", true},
              {"retryCount", 2},
              {"retryWaitSeconds", 2},
              {"waitForShare", false},
              {"dryRunListOnly", false},
              {"createZeroByteTree", false},
              {"excludeExtraLogs", false},
              {"excludeLonelyFiles", false},
              {"excludeFilePatterns", ""},
              {"excludeDirPatterns", ""},
              {"minFileSizeBytes", ""},
              {"maxFileSizeBytes", ""},
              {"maxFileAge", ""},
              {"minFileAge", ""},
              {"verifyHashPostCopy", false},
              {"hashAlgorithm", 0},
              {"writeSidecars", false}};
}
QStringList RobocopyProfile::validate() const {
    QStringList errors;
    for (const auto &[key, maximum] : QList<QPair<QString, int>>{
             {"transferMode", 5}, {"collisionPolicy", 4}, {"accessMode", 3}, {"hashAlgorithm", 2}})
        if (number(key) < 0 || number(key) > maximum)
            errors << "Invalid choice: " + key;
    if (number("threadCount") < 1 || number("threadCount") > 128)
        errors << "Thread count must be between 1 and 128.";
    if (number("retryCount") < 0 || number("retryWaitSeconds") < 0)
        errors << "Retry values cannot be negative.";
    if (flag("enableBandwidthLimit") && (number("interPacketGapMs") < 1 || number("interPacketGapMs") > 1000))
        errors << "Packet gap must be 1–1000 ms.";
    if (!flag("copyData") && !flag("copyAttributes") && !flag("copyTimestamps") && !flag("copySecurityAcl") &&
        !flag("copyOwner") && !flag("copyAuditing"))
        errors << "Select at least one file attribute to copy.";
    if (flag("fixSecurityOnExisting") && !flag("copySecurityAcl"))
        errors << "Security repair requires Copy security ACLs.";
    if (flag("verifyHashPostCopy") && !flag("dryRunListOnly") &&
        (mode() == TransferMode::MoveTree || mode() == TransferMode::MoveFiles ||
         flag("createZeroByteTree") || !flag("copyData")))
        errors << "Hash verification requires a data copy with source files retained.";
    for (auto key : {"minFileSizeBytes", "maxFileSizeBytes"}) {
        bool ok = true;
        if (!string(key).isEmpty())
            string(key).toULongLong(&ok);
        if (!ok)
            errors << QString("%1 must be a non-negative byte count.").arg(key);
    }
    if (!string("maxFileSizeBytes").isEmpty() &&
        string("minFileSizeBytes").toULongLong() > string("maxFileSizeBytes").toULongLong())
        errors << "Minimum file size exceeds maximum.";
    for (auto key : {"maxFileAge", "minFileAge"})
        if (!string(key).isEmpty() && !QRegularExpression("^\\d{1,8}$").match(string(key)).hasMatch())
            errors << QString("%1 must be days or YYYYMMDD.").arg(key);
    for (auto it = values.cbegin(); it != values.cend(); ++it)
        if (it.value().metaType().id() == QMetaType::QString &&
            it.value().toString().contains(QRegularExpression("[\\r\\n\\x00\"]")))
            errors << "Options cannot contain double quotes, line breaks or NUL characters. Separate "
                      "exclusion patterns with semicolons.";
    return errors;
}
RobocopyProfile RobocopyProfile::preset(int index) {
    RobocopyProfile p;
    if (index == 1) {
        p.values["accessMode"] = 1;
        p.values["threadCount"] = 64;
    }
    if (index == 2) {
        p.values["transferMode"] = 5;
        p.values["accessMode"] = 3;
    }
    if (index == 3) {
        p.values["accessMode"] = 3;
        p.values["copySecurityAcl"] = true;
        p.values["copyOwner"] = true;
        p.values["copyAuditing"] = true;
    }
    return p;
}
QVariantList RobocopyProfile::schema() {
    QVariantList groups;
    auto choice = [&](QString title, QString key, QStringList labels, QStringList flags, QString help) {
        QVariantList options;
        for (int i = 0; i < labels.size(); ++i)
            options << QVariantMap{{"label", labels[i]}, {"flag", flags[i]}};
        groups << QVariantMap{
            {"title", title}, {"key", key}, {"kind", "radio"}, {"options", options}, {"help", help}};
    };
    choice("A / Transfer mode", "transferMode",
           {"All subfolders, including empty", "Non-empty subfolders", "Root files only",
            "Move files and folders", "Move files, retain folders", "Mirror destination"},
           {"/E", "/S", "/LEV:1", "/E /MOVE", "/E /MOV", "/MIR"},
           "Move deletes source items after copying. Mirror deletes destination extras. Review a dry run "
           "first.");
    choice("B / Existing files", "collisionPolicy",
           {"Smart delta copy", "Keep newer destination files", "Skip all existing files",
            "Force overwrite everything", "Ask for each existing file"},
           {"DELTA", "/XO", "/XC /XN /XO", "/IS /IT", "DEFAULT"},
           "The default asks you to replace, skip, or make a duplicate of each existing file. Other modes apply their rule without prompting.");
    choice("C / Access and resilience", "accessMode",
           {"Standard fast I/O", "Restartable network transfer", "Backup privileges",
            "Restartable with backup fallback"},
           {"DEFAULT", "/Z", "/B", "/ZB"},
           "Backup modes require a Windows token with backup and restore privileges. Run as administrator if "
           "needed.");
    auto fields = [&](QString title, std::initializer_list<QVariantMap> items) {
        QVariantList options;
        for (const auto &item : items)
            options << item;
        groups << QVariantMap{{"title", title}, {"kind", "fields"}, {"options", options}};
    };
    auto f = [](QString key, QString label, QString flag, QString type = "bool", QString help = "") {
        return QVariantMap{{"key", key}, {"label", label}, {"flag", flag}, {"type", type}, {"help", help}};
    };
    fields("D / Performance", {f("enableMultithreading", "Multithreaded transfers", "/MT"),
                               f("threadCount", "Worker threads (1–128)", "/MT:n", "int"),
                               f("unbufferedIo", "Unbuffered disk I/O", "/J"),
                               f("smbCompression", "Request SMB compression", "/COMPRESS"),
                               f("disableOffload", "Disable copy offload", "/NOOFFLOAD"),
                               f("enableBandwidthLimit", "Inter-packet delay", "/IPG", "bool",
                                 "Enabling this disables /MT because Robocopy cannot combine them."),
                               f("interPacketGapMs", "Packet gap in milliseconds", "/IPG:n", "int")});
    fields("E / Attributes and links",
           {f("copyData", "Copy file data", "D"), f("copyAttributes", "Copy attributes", "A"),
            f("copyTimestamps", "Copy timestamps", "T"), f("copySecurityAcl", "Copy security ACLs", "S"),
            f("copyOwner", "Copy ownership", "O"), f("copyAuditing", "Copy auditing information", "U"),
            f("preserveDirectoryTimestamps", "Preserve directory timestamps", "/DCOPY:DAT"),
            f("excludeJunctionLoops", "Exclude junctions", "/XJ"),
            f("fatTwoSecondTolerance", "FAT / NAS timestamp tolerance", "/FFT"),
            f("copySymlinksAsLinks", "Copy symlinks as links", "/SL"),
            f("fixSecurityOnExisting", "Repair security on skipped files", "/SECFIX"),
            f("fixTimestampsOnExisting", "Repair timestamps on skipped files", "/TIMFIX")});
    fields("F / Retries and filters",
           {f("fastFailSafeRetries", "Use custom retry limits", "/R /W", "bool",
              "When off, Robocopy defaults to one million retries. Keep this enabled for normal use."),
            f("retryCount", "Retries per failed file", "/R:n", "int"),
            f("retryWaitSeconds", "Seconds between retries", "/W:n", "int"),
            f("waitForShare", "Wait for network share", "/TBD"),
            f("dryRunListOnly", "Dry run — preview only", "/L"),
            f("createZeroByteTree", "Create zero-byte file skeleton", "/CREATE"),
            f("excludeExtraLogs", "Exclude extra destination items", "/XX"),
            f("excludeLonelyFiles", "Only sync existing files", "/XL"),
            f("excludeFilePatterns", "Exclude files (semicolon separated)", "/XF", "text"),
            f("excludeDirPatterns", "Exclude folders (semicolon separated)", "/XD", "text"),
            f("minFileSizeBytes", "Minimum size in bytes", "/MIN", "text"),
            f("maxFileSizeBytes", "Maximum size in bytes", "/MAX", "text"),
            f("maxFileAge", "Maximum age (days or YYYYMMDD)", "/MAXAGE", "text"),
            f("minFileAge", "Minimum age (days or YYYYMMDD)", "/MINAGE", "text")});
    fields("G / Readback verification",
           {f("verifyHashPostCopy", "Verify destination after copy", "HASH", "bool",
              "Reads both files after Robocopy succeeds. Unsupported for moves and skeleton copies."),
            f("writeSidecars", "Write hash sidecars to destination", "SIDECAR", "bool",
              "Creates .xxh3, .blake3 or .sha256 files alongside verified files.")});
    return groups;
}
} // namespace strata
