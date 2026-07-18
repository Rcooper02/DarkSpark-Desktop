// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_INTERFACES_ITELEMETRYPROVIDER_HPP
#define DARKSPARK_INTERFACES_ITELEMETRYPROVIDER_HPP

#include <QObject>

#include "models/MetricSample.hpp"

namespace darkspark::interfaces {

/// Abstract contract for a source of telemetry samples.
///
/// One provider represents one telemetry source: it produces samples for a
/// single metric, so `currentSample()` takes no arguments.
///
/// QObject-based so implementations can emit Qt signals, which is idiomatic in
/// this Qt application. Only model values cross this boundary: no widget types,
/// no UI types, no platform-specific types. The contract says nothing about how
/// samples are obtained — no polling mechanism, cadence, data source, or
/// threading strategy is implied here. Those are implementation concerns.
///
/// Errors and missing data are represented through MetricState (Unavailable /
/// Stale) on the sample itself, never through exceptions or error returns.
///
/// Ownership: a QObject owned by its Qt parent. The composition root
/// (Application) owns the provider; no UI object owns it, and it owns no UI
/// object.
///
/// Threading: GUI thread only for this slice. Connections are direct, so no
/// metatype registration is required. If a queued connection is ever needed,
/// MetricSample would have to be registered with the Qt metatype system.
class ITelemetryProvider : public QObject {
    Q_OBJECT

public:
    explicit ITelemetryProvider(QObject* parent = nullptr) : QObject(parent) {}
    ~ITelemetryProvider() override = default;

    ITelemetryProvider(const ITelemetryProvider&) = delete;
    ITelemetryProvider& operator=(const ITelemetryProvider&) = delete;
    ITelemetryProvider(ITelemetryProvider&&) = delete;
    ITelemetryProvider& operator=(ITelemetryProvider&&) = delete;

    /// Begin (or resume) sampling.
    ///
    /// Idempotent: calling start() while already running is a no-op — it does
    /// not restart sampling and does not reset any implementation state.
    /// Startup problems are not reported here; they surface as samples in the
    /// Unavailable state.
    virtual void start() = 0;

    /// Stop sampling.
    ///
    /// Idempotent: calling stop() while already stopped is a no-op. Stopping
    /// ends sample production but does not destroy the provider: the object
    /// remains valid and currentSample() continues to answer.
    virtual void stop() = 0;

    /// The most recent sample produced by this provider.
    ///
    /// Never fabricates data. Before the first start(), and whenever no usable
    /// value exists, the returned sample carries MetricState::Unavailable with
    /// no numeric value. After stop(), the last sample produced remains
    /// queryable.
    [[nodiscard]] virtual models::MetricSample currentSample() const = 0;

signals:
    /// Emitted whenever a new sample is produced, including transitions into
    /// the Unavailable and Stale states, so consumers observe state changes and
    /// not only value changes.
    void readingChanged(const darkspark::models::MetricSample& sample);
};

}  // namespace darkspark::interfaces

#endif  // DARKSPARK_INTERFACES_ITELEMETRYPROVIDER_HPP
