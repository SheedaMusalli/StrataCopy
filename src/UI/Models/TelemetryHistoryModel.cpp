#include "TelemetryHistoryModel.hpp"
namespace strata {
QVariantList TelemetryHistoryModel::points() const {
    QVariantList out;
    out.reserve(qsizetype(m_points.size()));
    for (const auto &p : m_points)
        out << QVariantMap{{"read", p.first}, {"write", p.second}};
    return out;
}
void TelemetryHistoryModel::append(double read, double write) {
    if (m_points.size() >= 600)
        m_points.pop_front();
    m_points.emplace_back(read, write);
    emit changed();
}
void TelemetryHistoryModel::clear() {
    m_points.clear();
    emit changed();
}
} // namespace strata
