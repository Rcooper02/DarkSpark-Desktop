// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_INTERFACES_ITELEMETRYPROVIDER_HPP
#define DARKSPARK_INTERFACES_ITELEMETRYPROVIDER_HPP

#include <QList>
#include <QObject>

#include "models/MetricSample.hpp"

namespace darkspark::interfaces {

/// Abstract contract for a source of telemetry samples.
///
/// A provider represents one telemetry SOURCE, which may expose more than one
/// sensor within a category: for example a CPU thermal provider can emit a
/// package temperature plus one sample per CCD. `currentSamples()` therefore
/// returns a collection, and single-sensor providers simply return a
/// one-element list.
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
    /// remains valid and currentSamples() continues to answer.
    virtual void stop() = 0;

    /// The most recent sample for each sensor this provider exposes.
    ///
    /// Never fabricates data. Before the first start(), and whenever no usable
    /// value exists for a sensor, that sensor's sample carries
    /// MetricState::Unavailable with no numeric value. After stop(), the last
    /// samples produced remain queryable. A single-sensor provider returns a
    /// one-element list; a provider that has discovered no sensors at all may
    /// return an empty list.
    [[nodiscard]] virtual QList<models::MetricSample> currentSamples() const = 0;

signals:
    /// Emitted whenever a new sample is produced, including transitions into
    /// the Unavailable and Stale states, so consumers observe state changes and
    /// not only value changes.
    void readingChanged(const darkspark::models::MetricSample& sample);
};

}  // namespace darkspark::interfaces

#endif  // DARKSPARK_INTERFACES_ITELEMETRYPROVIDER_HPP
