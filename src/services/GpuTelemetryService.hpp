// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_SERVICES_GPUTELEMETRYSERVICE_HPP
#define DARKSPARK_SERVICES_GPUTELEMETRYSERVICE_HPP

#include <QTimer>

#include <functional>
#include <optional>
#include <string>

#include "interfaces/ITelemetryProvider.hpp"
#include "models/MetricSample.hpp"

namespace darkspark::services {

// Forward-declared in the enclosing services namespace (NOT inside detail) so
// the detail helpers below refer to this class, services::GpuTelemetryService,
// rather than declaring a distinct detail::GpuTelemetryService.
class GpuTelemetryService;

namespace detail {

/// Injected collaborators for deterministic testing.
///
/// `readBusyPercent` returns the raw contents of the amdgpu
/// `gpu_busy_percent` sysfs file (a small integer string, e.g. "73\n"), or
/// nullopt if the source could not be read (missing device, permission, etc.).
/// `now` returns a monotonic timestamp in milliseconds. Both have production
/// defaults; tests supply scripted versions so no test depends on real sysfs,
/// the real clock, or a live GPU.
struct GpuUtilizationSources {
    std::function<std::optional<std::string>()> readBusyPercent;
    std::function<models::MonotonicTimestamp()> now;
};

[[nodiscard]] GpuTelemetryService* makeWithSources(GpuUtilizationSources sources,
                                                   QObject* parent);
void pollOnceForTest(GpuTelemetryService& service);

}  // namespace detail

/// GPU utilization telemetry for AMD (amdgpu), sourced from the driver's
/// `gpu_busy_percent` sysfs file.
///
/// Implements ITelemetryProvider for MetricId::GpuTotalUtilization. Unlike CPU
/// utilization (a delta of /proc/stat counters), amdgpu already exposes an
/// instantaneous busy percentage in [0, 100], so no baseline or delta is
/// needed: each poll reads and reports the current value directly. A missing
/// device or malformed contents yields Unavailable (or Stale if a prior valid
/// value exists), never a throw.
///
/// Production source path (resolved at construction): the first readable
///   /sys/class/drm/card*/device/gpu_busy_percent
/// See GpuTelemetryService.cpp for the exact resolution rule.
///
/// Ownership: a QObject owned by its Qt parent. Threading: GUI thread only; the
/// sysfs read is a bounded non-blocking pseudo-file read.
class GpuTelemetryService : public interfaces::ITelemetryProvider {
    Q_OBJECT

public:
    explicit GpuTelemetryService(QObject* parent = nullptr);
    ~GpuTelemetryService() override;

    void start() override;
    void stop() override;

    [[nodiscard]] QList<models::MetricSample> currentSamples() const override;

private:
    friend GpuTelemetryService* detail::makeWithSources(
        detail::GpuUtilizationSources, QObject*);
    friend void detail::pollOnceForTest(GpuTelemetryService&);

    GpuTelemetryService(detail::GpuUtilizationSources sources, QObject* parent);

    void poll();
    [[nodiscard]] models::MetricSample computeSample();

    detail::GpuUtilizationSources sources_;
    QTimer* timer_;
    std::optional<double> lastValidValue_;
    models::MetricSample current_;
};

}  // namespace darkspark::services

#endif  // DARKSPARK_SERVICES_GPUTELEMETRYSERVICE_HPP
