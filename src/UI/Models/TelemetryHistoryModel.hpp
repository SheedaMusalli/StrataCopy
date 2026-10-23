#pragma once
#include <QAbstractListModel>
#include <QVariantList>
#include <deque>
namespace strata {
class TelemetryHistoryModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList points READ points NOTIFY changed)
  public:
    using QObject::QObject;
    QVariantList points() const;
    void append(double read, double write);
    void clear();
  signals:
    void changed();

  private:
    std::deque<QPair<double, double>> m_points;
};
} // namespace strata
