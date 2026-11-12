#include "TelemetryHistoryModel.hpp"
namespace strata {
QVariantList TelemetryHistoryModel::points() const {
    QVariantList out;
    out.reserve(qsizetype(m_points.size()));
    for (const auto write : m_points)
        out << write;
    return out;
}
void TelemetryHistoryModel::append(double write) {
    if (m_points.size() >= 240)
        m_points.pop_front();
    m_points.emplace_back(write);
    emit changed();
}
void TelemetryHistoryModel::clear() {
    m_points.clear();
    emit changed();
}
} // namespace strata
