// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_DECK_INSTRUMENTS_STORAGEINSTRUMENTMODELADAPTER_HPP
#define DARKSPARK_DECK_INSTRUMENTS_STORAGEINSTRUMENTMODELADAPTER_HPP

#include "deck/instruments/StorageInstrumentModel.hpp"
#include "models/MetricSample.hpp"

namespace darkspark::deck::instruments {

/// Translates storage telemetry samples into a StorageInstrumentModel.
///
/// Joins all six approved storage metrics into the model -- including
/// temperature and throughput, which the V1 face does not display but which are
/// retained for later detail views. Ignores every other subsystem's metrics.
/// Never fabricates a value (Unavailable -> Absent), never invents a percentage.
///
///   Fresh       -> Live
///   Stale       -> LastKnown
///   Unavailable -> Absent
class StorageInstrumentModelAdapter {
public:
    bool apply(const models::MetricSample& sample);
    [[nodiscard]] const StorageInstrumentModel& model() const { return model_; }
    void reset() { model_ = StorageInstrumentModel{}; }

private:
    static ValueAvailability availabilityFor(models::MetricState state);
    StorageInstrumentModel model_{};
};

}  // namespace darkspark::deck::instruments

#endif  // DARKSPARK_DECK_INSTRUMENTS_STORAGEINSTRUMENTMODELADAPTER_HPP
