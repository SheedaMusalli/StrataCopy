#pragma once
#include <QByteArray>
#include <QList>
#include <QMetaType>
#include <QString>
#include <functional>
namespace strata {
struct FileEntry {
    QString path, classification, state, hash;
    quint64 bytes = 0;
};
class StdoutPipeParser {
  public:
    using Sink = std::function<void(const QString &, const FileEntry *)>;
    explicit StdoutPipeParser(Sink sink) : m_sink(std::move(sink)) {}
    void feed(const QByteArray &bytes);
    void finish();
    static bool parseLine(const QString &, FileEntry &);

  private:
    void consume(bool finish);
    QByteArray m_pending;
    Sink m_sink;
};
} // namespace strata
Q_DECLARE_METATYPE(strata::FileEntry)
