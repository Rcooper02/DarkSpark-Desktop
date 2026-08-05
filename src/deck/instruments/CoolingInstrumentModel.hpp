// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_DECK_INSTRUMENTS_COOLINGINSTRUMENTMODEL_HPP
#define DARKSPARK_DECK_INSTRUMENTS_COOLINGINSTRUMENTMODEL_HPP

#include "deck/instruments/CpuInstrumentModel.hpp"  // for ValueAvailability

namespace darkspark::deck::instruments {

/// The values a CoolingInstrument renders.
///
/// Role-based and hardware-agnostic: "primary" and "secondary" are logical
/// roles the telemetry layer already resolved (a pump, a fan, whatever discovery
/// chose), so this model is independent of how many fans or pumps exist. The
/// primary is a RAW RPM figure -- never a fabricated percentage. The secondary
/// is either a coolant temperature (Celsius) or a second fan's RPM.
struct CoolingInstrumentModel {
    /// Primary cooling activity, in RPM. Raw units, never a percentage.
    double primaryRpm = 0.0;
    ValueAvailability primaryAvailability = ValueAvailability::Absent;

    /// Secondary coolant temperature (Celsius), if a loop exposes one.
    double secondaryTempCelsius = 0.0;
    ValueAvailability secondaryTempAvailability = ValueAvailability::Absent;

    /// Secondary fan RPM, if the secondary is a fan rather than a coolant temp.
    double secondaryRpm = 0.0;
    ValueAvailability secondaryRpmAvailability = ValueAvailability::Absent;
};

}  // namespace darkspark::deck::instruments

#endif  // DARKSPARK_DECK_INSTRUMENTS_COOLINGINSTRUMENTMODEL_HPP
