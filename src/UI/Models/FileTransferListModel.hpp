#pragma once
#include "Core/StdoutPipeParser.hpp"
#include <QAbstractListModel>
#include <QSortFilterProxyModel>
namespace strata {
class FileTransferListModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(QVariantMap counters READ counters NOTIFY countChanged)
    Q_PROPERTY(quint64 omittedCount READ omittedCount NOTIFY countChanged)
  public:
    enum Role { PathRole = Qt::UserRole + 1, ClassificationRole, StateRole, BytesRole, HashRole };
    using QAbstractListModel::QAbstractListModel;
    int rowCount(const QModelIndex &parent = {}) const override {
        return parent.isValid() ? 0 : int(m_rows.size());
    }
    QVariant data(const QModelIndex &, int) const override;
    QHash<int, QByteArray> roleNames() const override;
    void append(QList<FileEntry> entries);
    void clear();
    QVariantMap counters() const {
        return m_counters;
    }
    void settle(bool success, bool dryRun);
    quint64 omittedCount() const {
        return m_omitted;
    }
  signals:
    void countChanged();

  private:
    QList<FileEntry> m_rows;
    QVariantMap m_counters;
    quint64 m_retainedBytes = 0;
    quint64 m_omitted = 0;
};
class FileFilterModel : public QSortFilterProxyModel {
    Q_OBJECT
  public:
    using QSortFilterProxyModel::QSortFilterProxyModel;
    void setCategory(QString category) {
        m_category = std::move(category);
        invalidateFilter();
    }
    void setSearch(QString search) {
        m_search = std::move(search);
        invalidateFilter();
    }

  protected:
    bool filterAcceptsRow(int, const QModelIndex &) const override;

  private:
    QString m_category, m_search;
};
} // namespace strata
