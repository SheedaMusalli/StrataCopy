#include "KernelTelemetryPoller.hpp"
#include <QtGlobal>
namespace strata {
void KernelTelemetryPoller::reset(HANDLE process) {
    m_process = process;
    m_previous = {};
    GetProcessIoCounters(m_process, &m_previous);
    m_time = std::chrono::steady_clock::now();
}
TelemetrySample KernelTelemetryPoller::sample() {
    IO_COUNTERS now{};
    const auto time = std::chrono::steady_clock::now();
    if (!m_process || !GetProcessIoCounters(m_process, &now))
        return {};
    double seconds = std::chrono::duration<double>(time - m_time).count();
    if (seconds <= 0)
        return {};
    TelemetrySample s;
    s.readMbps = (now.ReadTransferCount - m_previous.ReadTransferCount) / seconds / 1e6;
    s.writeMbps = (now.WriteTransferCount - m_previous.WriteTransferCount) / seconds / 1e6;
    s.iops = (now.ReadOperationCount - m_previous.ReadOperationCount + now.WriteOperationCount -
              m_previous.WriteOperationCount) /
             seconds;
    s.readBytes = now.ReadTransferCount;
    s.writeBytes = now.WriteTransferCount;
    m_previous = now;
    m_time = time;
    return s;
}
} // namespace strata
