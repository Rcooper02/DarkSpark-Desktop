// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_DECK_INSTRUMENTS_COOLINGINSTRUMENTMODELADAPTER_HPP
#define DARKSPARK_DECK_INSTRUMENTS_COOLINGINSTRUMENTMODELADAPTER_HPP

#include "deck/instruments/CoolingInstrumentModel.hpp"
#include "models/MetricSample.hpp"

namespace darkspark::deck::instruments {

/// Translates cooling telemetry samples into a CoolingInstrumentModel.
///
/// Because the telemetry contract is ROLE-BASED (the service already decided
/// what "primary" and "secondary" mean), this adapter is simple and topology-
/// independent: it maps CoolingPrimary -> primary, CoolingSecondary -> secondary
/// fan, CoolingCoolantTemp -> secondary coolant temperature, and ignores all
/// other subsystems' metrics. It never fabricates a value (Unavailable -> Absent)
/// and never invents a percentage from RPM.
///
///   Fresh       -> Live
///   Stale       -> LastKnown
///   Unavailable -> Absent
class CoolingInstrumentModelAdapter {
public:
    bool apply(const models::MetricSample& sample);
    [[nodiscard]] const CoolingInstrumentModel& model() const { return model_; }
    void reset() { model_ = CoolingInstrumentModel{}; }

private:
    static ValueAvailability availabilityFor(models::MetricState state);
    CoolingInstrumentModel model_{};
};

}  // namespace darkspark::deck::instruments

#endif  // DARKSPARK_DECK_INSTRUMENTS_COOLINGINSTRUMENTMODELADAPTER_HPP
