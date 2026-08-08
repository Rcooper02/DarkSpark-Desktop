// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_DECK_INSTRUMENTS_NETWORKINSTRUMENTMODELADAPTER_HPP
#define DARKSPARK_DECK_INSTRUMENTS_NETWORKINSTRUMENTMODELADAPTER_HPP

#include "deck/instruments/NetworkInstrumentModel.hpp"
#include "models/MetricSample.hpp"

namespace darkspark::deck::instruments {

/// Translates network telemetry samples into a NetworkInstrumentModel.
///
/// Joins all five approved network metrics into the model -- including
/// cumulative bytes and link state, which the V1 face does not display but which
/// are retained for later detail views. Ignores every other subsystem's metrics.
/// Link state is interpreted by MetricId from its numeric value (0 = Down,
/// 1 = Up); there is no MetricUnit for it. Never fabricates a value
/// (Unavailable -> Absent), never invents throughput.
///
///   Fresh       -> Live
///   Stale       -> LastKnown
///   Unavailable -> Absent
class NetworkInstrumentModelAdapter {
public:
    bool apply(const models::MetricSample& sample);
    [[nodiscard]] const NetworkInstrumentModel& model() const { return model_; }
    void reset() { model_ = NetworkInstrumentModel{}; }

private:
    static ValueAvailability availabilityFor(models::MetricState state);
    NetworkInstrumentModel model_{};
};

}  // namespace darkspark::deck::instruments

#endif  // DARKSPARK_DECK_INSTRUMENTS_NETWORKINSTRUMENTMODELADAPTER_HPP
