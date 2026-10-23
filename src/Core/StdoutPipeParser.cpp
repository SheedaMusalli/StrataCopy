#include "StdoutPipeParser.hpp"
#include <QRegularExpression>
namespace strata {
void StdoutPipeParser::feed(const QByteArray &bytes) {
    m_pending += bytes;
    consume(false);
}
void StdoutPipeParser::finish() {
    consume(true);
}
void StdoutPipeParser::consume(bool final) {
    // Robocopy /UNICODE emits little-endian UTF-16. Preserve split code units and lines.
    qsizetype consumed = 0;
    auto emitLine = [&](qsizetype end) {
        QString line;
        line.reserve((end - consumed) / 2);
        for (qsizetype i = consumed; i + 1 < end; i += 2) {
            auto ch = char16_t(quint8(m_pending[i]) | (quint16(quint8(m_pending[i + 1])) << 8));
            if (ch != u'\r' && ch != u'\ufeff')
                line += QChar(ch);
        }
        if (!line.trimmed().isEmpty()) {
            FileEntry entry;
            const bool recognized = parseLine(line, entry);
            m_sink(line, recognized ? &entry : nullptr);
        }
    };
    for (qsizetype i = 0; i + 1 < m_pending.size(); i += 2)
        if (m_pending[i] == '\n' && m_pending[i + 1] == '\0') {
            emitLine(i);
            consumed = i + 2;
        }
    if (final && consumed < m_pending.size()) {
        emitLine(m_pending.size());
        consumed = m_pending.size();
    }
    m_pending.remove(0, consumed);
    // Bound a malformed or hostile line without growing an unbounded buffer.
    if (m_pending.size() > 1024 * 1024) {
        m_sink("Output line exceeded 1 MiB; truncated.", nullptr);
        m_pending.clear();
    }
}
bool StdoutPipeParser::parseLine(const QString &line, FileEntry &out) {
    static const QRegularExpression error(R"(\bERROR\s+\d+\b)", QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression file(
        R"(^\s*(.*?)\s+(\d+)\s+(?:\d{4}[/\-]\d{2}[/\-]\d{2}\s+\d{2}:\d{2}:\d{2}\s+)?((?:[A-Za-z]:[\\/]|\\\\).+?)\s*$)");
    if (error.match(line).hasMatch()) {
        out = {line.trimmed(), "Error", "Failed", {}, 0};
        return true;
    }
    const auto match = file.match(line);
    if (!match.hasMatch())
        return false;
    out.classification = match.captured(1).trimmed();
    out.bytes = match.captured(2).toULongLong();
    out.path = match.captured(3);
    const auto category = out.classification.toLower();
    // Entries are observations emitted by Robocopy, not a per-file completion receipt.
    if (category.contains("extra"))
        out.state = "Extra";
    else if (category == "same" || category == "tweaked" || category == "named" || category.contains("skip"))
        out.state = "Skipped";
    else
        out.state = "Listed";
    return true;
}
} // namespace strata
