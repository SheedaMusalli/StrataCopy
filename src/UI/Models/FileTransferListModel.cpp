#include "FileTransferListModel.hpp"
namespace strata {
QVariant FileTransferListModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return {};
    const auto &e = m_rows[index.row()];
    switch (role) {
    case PathRole:
        return e.path;
    case ClassificationRole:
        return e.classification;
    case StateRole:
        return e.state;
    case BytesRole:
        return QVariant::fromValue(e.bytes);
    case HashRole:
        return e.hash;
    default:
        return {};
    }
}
QHash<int, QByteArray> FileTransferListModel::roleNames() const {
    return {{PathRole, "filePath"},
            {ClassificationRole, "classification"},
            {StateRole, "fileState"},
            {BytesRole, "fileBytes"},
            {HashRole, "fileHash"}};
}
void FileTransferListModel::append(QList<FileEntry> entries) {
    constexpr qsizetype limit = 1000000;
    if (entries.isEmpty())
        return;
    if (m_rows.size() >= limit) {
        m_omitted += entries.size();
        emit countChanged();
        return;
    }
    if (entries.size() > limit - m_rows.size()) {
        m_omitted += entries.size() - (limit - m_rows.size());
        entries.resize(limit - m_rows.size());
    }
    qsizetype accepted = 0;
    for (const auto &entry : entries) {
        const auto bytes =
            quint64(sizeof(FileEntry)) + quint64(entry.path.size() + entry.classification.size() +
                                                 entry.state.size() + entry.hash.size()) *
                                             2;
        if (m_retainedBytes + bytes > 256ull * 1024 * 1024)
            break;
        m_retainedBytes += bytes;
        ++accepted;
    }
    m_omitted += entries.size() - accepted;
    if (!accepted) {
        emit countChanged();
        return;
    }
    if (accepted < entries.size())
        entries.resize(accepted);
    beginInsertRows({}, int(m_rows.size()), int(m_rows.size() + entries.size() - 1));
    for (auto &entry : entries) {
        m_counters[entry.state] = m_counters.value(entry.state).toULongLong() + 1;
        m_rows.append(std::move(entry));
    }
    endInsertRows();
    emit countChanged();
}
void FileTransferListModel::clear() {
    beginResetModel();
    m_rows.clear();
    m_rows.squeeze();
    m_counters.clear();
    m_retainedBytes = 0;
    m_omitted = 0;
    endResetModel();
    emit countChanged();
}
void FileTransferListModel::settle(bool success, bool dryRun) {
    if (dryRun || !success)
        return;
    // A successful batch confirms that Robocopy completed its listed copy operations.
    for (auto &e : m_rows)
        if (e.state == "Listed")
            e.state = "Copied";
    m_counters["Copied"] = m_counters.value("Listed", 0);
    m_counters.remove("Listed");
    if (!m_rows.isEmpty())
        emit dataChanged(index(0), index(int(m_rows.size() - 1)), {StateRole});
    emit countChanged();
}
bool FileFilterModel::filterAcceptsRow(int row, const QModelIndex &parent) const {
    const auto idx = sourceModel()->index(row, 0, parent);
    const auto state = sourceModel()->data(idx, FileTransferListModel::StateRole).toString();
    return (m_category.isEmpty() || state == m_category || (m_category == "Copied" && state == "Listed")) &&
           (m_search.isEmpty() || sourceModel()
                                      ->data(idx, FileTransferListModel::PathRole)
                                      .toString()
                                      .contains(m_search, Qt::CaseInsensitive));
}
} // namespace strata
