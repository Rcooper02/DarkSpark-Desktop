// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_DECK_INSTRUMENTS_NETWORKINSTRUMENTMODEL_HPP
#define DARKSPARK_DECK_INSTRUMENTS_NETWORKINSTRUMENTMODEL_HPP

#include <string>

#include "deck/instruments/CpuInstrumentModel.hpp"  // for ValueAvailability

namespace darkspark::deck::instruments {

/// Link state of the active interface, interpreted from the numeric
/// NetworkLinkState metric (0 = down, 1 = up) by MetricId -- there is no
/// dedicated MetricUnit for it.
enum class NetworkLinkState { Unknown, Down, Up };

/// The values a NetworkInstrument holds.
///
/// The model deliberately retains the COMPLETE set even though the V1 face shows
/// only two of them (download as primary, upload as secondary). Interface
/// identity, link state, and cumulative byte totals stay live here so a later
/// detail or personality view needs no telemetry redesign -- the exact
/// "retain everything, display two" pattern Storage established.
///
/// All values are raw and truthful with per-value availability; the instrument
/// never fabricates (an Absent value is not rendered as a real reading).
struct NetworkInstrumentModel {
    /// Download throughput, bytes/sec. The primary, displayed value.
    double receiveBytesPerSec = 0.0;
    ValueAvailability receiveAvailability = ValueAvailability::Absent;

    /// Upload throughput, bytes/sec. The secondary, displayed value.
    double transmitBytesPerSec = 0.0;
    ValueAvailability transmitAvailability = ValueAvailability::Absent;

    /// Cumulative received/transmitted bytes. Retained for later detail views;
    /// not on the V1 face.
    double receivedBytes = 0.0;
    ValueAvailability receivedAvailability = ValueAvailability::Absent;
    double transmittedBytes = 0.0;
    ValueAvailability transmittedAvailability = ValueAvailability::Absent;

    /// Link state. Retained for later detail views; not on the V1 face.
    NetworkLinkState linkState = NetworkLinkState::Unknown;
    ValueAvailability linkAvailability = ValueAvailability::Absent;

    /// Retained interface identity (populated by wiring, not by the metric
    /// stream). Not displayed in V1; kept for future UI expansion.
    std::string interfaceName;
    bool isDefaultRoute = false;
    bool isPhysical = false;
    bool isLoopback = false;
};

}  // namespace darkspark::deck::instruments

#endif  // DARKSPARK_DECK_INSTRUMENTS_NETWORKINSTRUMENTMODEL_HPP
