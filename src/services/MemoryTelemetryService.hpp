// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_SERVICES_MEMORYTELEMETRYSERVICE_HPP
#define DARKSPARK_SERVICES_MEMORYTELEMETRYSERVICE_HPP

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

#include <QElapsedTimer>
#include <QList>
#include <QObject>

#include "interfaces/ITelemetryProvider.hpp"
#include "models/MetricSample.hpp"

class QTimer;

namespace darkspark::services {

class MemoryTelemetryService;

namespace detail {

/// Injected collaborators for deterministic testing.
///
/// `readMeminfo` returns the full contents of /proc/meminfo, or nullopt if the
/// source could not be read. `now` returns a monotonic timestamp in
/// milliseconds. Both have production defaults; tests supply scripted versions
/// so no test depends on the real /proc/meminfo, the real clock, or live memory
/// pressure.
///
/// The source yields file CONTENT rather than a path: a provider decides for
/// itself where its data comes from, which keeps this seam usable for sources
/// that are discovered rather than fixed.
struct MemorySources {
    std::function<std::optional<std::string>()> readMeminfo;
    std::function<models::MonotonicTimestamp()> now;
};

/// Internal access helper: constructs a MemoryTelemetryService with injected
/// sources. Not part of the public service API; it exists so tests can drive the
/// service deterministically without widening the production surface.
[[nodiscard]] MemoryTelemetryService* makeWithSources(MemorySources sources,
                                                      QObject* parent);

/// Internal access helper: performs exactly one sampling cycle synchronously.
///
/// Production code never calls this; the service polls itself on its own timer.
/// It exists so tests can step the state machine without running an event loop
/// or waiting on wall-clock time, and without making the private poll routine
/// part of the public API or the meta-object.
void pollOnceForTest(MemoryTelemetryService& service);

}  // namespace detail

/// System memory utilization telemetry for Linux, sourced from /proc/meminfo.
///
/// Implements ITelemetryProvider for MetricId::MemoryUtilization.
///
/// Unlike CPU utilization, memory utilization is an INSTANTANEOUS RATIO rather
/// than a delta between two snapshots. There is therefore no counter baseline,
/// no regression check, and no re-baselining: the first valid read already
/// yields a usable value and reports Fresh immediately. A stop/start cycle needs
/// no baseline reset because no measurement interval spans the gap. This is a
/// deliberate difference from CpuTelemetryService, not an oversight.
///
/// Utilization is computed from MemAvailable, not MemFree: MemFree excludes
/// reclaimable page cache, so a healthy system with a warm cache would otherwise
/// report near-total usage. MemAvailable is the kernel's own estimate of memory
/// obtainable without swapping, which is what "memory in use" means to a reader.
///
/// Expected read, parse, and calculation failures do not throw. They are
/// represented as telemetry states on the emitted sample (Unavailable when no
/// valid value exists, Stale when a previously valid value is retained).
///
/// Ownership: a QObject owned by its Qt parent; the composition root owns it.
/// Threading: GUI thread only.
class MemoryTelemetryService : public interfaces::ITelemetryProvider {
    Q_OBJECT

public:
    explicit MemoryTelemetryService(QObject* parent = nullptr);
    ~MemoryTelemetryService() override;

    /// Begin (or resume) sampling. Idempotent while already running.
    ///
    /// No baseline exists to clear, so a restart resumes reporting a valid value
    /// on the next poll rather than passing through an Unavailable warm-up.
    void start() override;

    /// Stop sampling. Idempotent. The last emitted sample remains queryable.
    void stop() override;

    /// The most recent emitted sample. Performs no I/O and never fabricates a
    /// value; before the first poll this is an Unavailable sample.
    [[nodiscard]] QList<models::MetricSample> currentSamples() const override;

private:
    friend MemoryTelemetryService* detail::makeWithSources(
        detail::MemorySources sources, QObject* parent);
    friend void detail::pollOnceForTest(MemoryTelemetryService& service);

    MemoryTelemetryService(detail::MemorySources sources, QObject* parent);

    void poll();

    void emitSample(const models::MetricSample& sample);

    /// Produce the sample for a failed read/parse/calculation: Stale when a
    /// previously valid value exists, otherwise Unavailable.
    [[nodiscard]] models::MetricSample makeFailureSample(const char* reason);

    detail::MemorySources sources_;
    QTimer* timer_;
    QElapsedTimer clock_;

    std::optional<double> lastValidValue_;
    std::optional<models::MetricSample> lastUsedBytes_;
    std::optional<models::MetricSample> lastTotalBytes_;
    models::MetricSample current_;
};

}  // namespace darkspark::services

#endif  // DARKSPARK_SERVICES_MEMORYTELEMETRYSERVICE_HPP
