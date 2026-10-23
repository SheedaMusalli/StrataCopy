#pragma once
#include <QtGlobal>
#include <chrono>
#include <windows.h>
namespace strata {
struct TelemetrySample {
    double readMbps = 0, writeMbps = 0, iops = 0;
    quint64 readBytes = 0, writeBytes = 0;
};
class KernelTelemetryPoller {
  public:
    void reset(HANDLE process);
    TelemetrySample sample();

  private:
    HANDLE m_process = nullptr; // Borrowed; the supervisor owns and outlives this poller.
    IO_COUNTERS m_previous{};
    std::chrono::steady_clock::time_point m_time;
};
} // namespace strata
