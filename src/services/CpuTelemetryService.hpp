// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_SERVICES_CPUTELEMETRYSERVICE_HPP
#define DARKSPARK_SERVICES_CPUTELEMETRYSERVICE_HPP

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

class CpuTelemetryService;

namespace detail {

/// Injected collaborators for deterministic testing.
///
/// `readStatLine` returns the aggregate CPU line of /proc/stat, or nullopt if
/// the source could not be read. `now` returns a monotonic timestamp in
/// milliseconds. Both have production defaults; tests supply scripted versions
/// so no test depends on the real /proc/stat, the real clock, or live CPU load.
struct TelemetrySources {
    std::function<std::optional<std::string>()> readStatLine;
    std::function<models::MonotonicTimestamp()> now;
};

/// Internal access helper: constructs a CpuTelemetryService with injected
/// sources. This is not part of the public service API and exists so tests can
/// drive the service deterministically without widening the production surface
/// or granting friendship to a specific test fixture.
[[nodiscard]] CpuTelemetryService* makeWithSources(TelemetrySources sources,
                                                   QObject* parent);

/// Internal access helper: performs exactly one sampling cycle synchronously.
///
/// Production code never calls this; the service polls itself on its own timer.
/// It exists so tests can step the state machine deterministically without
/// running an event loop or waiting on wall-clock time, and without making the
/// private poll routine part of the public API or the meta-object.
void pollOnceForTest(CpuTelemetryService& service);

}  // namespace detail

/// Aggregate CPU utilization telemetry for Linux, sourced from /proc/stat.
///
/// Implements ITelemetryProvider for the single metric
/// MetricId::CpuTotalUtilization. Parsing, snapshot handling, and the
/// utilization calculation are private implementation details of this class.
///
/// Sampling uses a service-owned QTimer on the application event-loop thread;
/// no worker thread is used. The /proc/stat read is a bounded, non-blocking
/// pseudo-file read, so performing it on the event-loop thread does not risk UI
/// stutter. That decision would need revisiting only if a future source could
/// block or became materially expensive.
///
/// Expected read, parse, counter, and calculation failures do not throw. They
/// are represented as telemetry states on the emitted sample (Unavailable when
/// no valid value exists, Stale when a previously valid value is retained).
///
/// Ownership: a QObject owned by its Qt parent; the composition root owns it.
/// Threading: GUI thread only.
class CpuTelemetryService : public interfaces::ITelemetryProvider {
    Q_OBJECT

public:
    explicit CpuTelemetryService(QObject* parent = nullptr);
    ~CpuTelemetryService() override;

    /// Begin (or resume) sampling. Idempotent while already running.
    ///
    /// Starting after a stop clears the counter baseline: the stopped interval
    /// is an unmeasured gap, and a delta spanning it would be labelled Fresh
    /// while actually describing historical time. The first poll after start
    /// therefore re-establishes the baseline and reports Unavailable; the next
    /// poll reports a genuine Fresh value. Calling start() while already
    /// running is a no-op and does NOT clear the baseline, because sampling has
    /// been continuous and no gap occurred.
    void start() override;

    /// Stop sampling. Idempotent. The last emitted sample remains queryable.
    void stop() override;

    /// The most recent emitted sample, as a one-element list (this provider
    /// exposes a single sensor). Performs no I/O and never fabricates a value;
    /// before the first poll this is an Unavailable sample.
    [[nodiscard]] QList<models::MetricSample> currentSamples() const override;

private:
    friend CpuTelemetryService* detail::makeWithSources(
        detail::TelemetrySources sources, QObject* parent);
    friend void detail::pollOnceForTest(CpuTelemetryService& service);

    CpuTelemetryService(detail::TelemetrySources sources, QObject* parent);

    /// A parsed /proc/stat aggregate snapshot. Only the first eight counters
    /// participate: guest and guest_nice are already included in user and nice
    /// respectively, so adding them would double-count.
    struct Snapshot {
        std::uint64_t idleAll{0};
        std::uint64_t busy{0};
        std::uint64_t total{0};
    };

    void poll();

    void emitSample(const models::MetricSample& sample);

    /// Produce the sample for a failed read/parse/calculation: Stale when a
    /// previously valid value exists, otherwise Unavailable. Always invalidates
    /// the baseline so recovery cannot delta across the gap.
    [[nodiscard]] models::MetricSample makeFailureSample(const char* reason);

    detail::TelemetrySources sources_;
    QTimer* timer_;
    QElapsedTimer clock_;

    std::optional<Snapshot> baseline_;
    std::optional<double> lastValidValue_;
    models::MetricSample current_;
};

}  // namespace darkspark::services

#endif  // DARKSPARK_SERVICES_CPUTELEMETRYSERVICE_HPP
