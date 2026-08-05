// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_DECK_INSTRUMENTS_STORAGEINSTRUMENTMODEL_HPP
#define DARKSPARK_DECK_INSTRUMENTS_STORAGEINSTRUMENTMODEL_HPP

#include "deck/instruments/CpuInstrumentModel.hpp"  // for ValueAvailability

namespace darkspark::deck::instruments {

/// The values a StorageInstrument holds.
///
/// The model deliberately retains the COMPLETE telemetry set -- utilization,
/// used, total, temperature, read throughput, write throughput -- even though
/// the V1 face renders only two of them (utilization as the primary, used/total
/// as the secondary). Temperature and throughput stay live here so a later
/// detail or personality view needs no telemetry redesign: the data already
/// flows and is already joined. Nothing is discarded because the current face
/// does not show it.
///
/// All values are raw and truthful with per-value availability; the instrument
/// never fabricates (an Absent value is not rendered as a real reading).
struct StorageInstrumentModel {
    /// Filesystem utilization, percent [0, 100]. The primary, displayed value.
    double utilizationPercent = 0.0;
    ValueAvailability utilizationAvailability = ValueAvailability::Absent;

    /// Used / total bytes -- the secondary "used / total" line.
    double usedBytes = 0.0;
    ValueAvailability usedAvailability = ValueAvailability::Absent;
    double totalBytes = 0.0;
    ValueAvailability totalAvailability = ValueAvailability::Absent;

    /// NVMe/drive temperature (Celsius). Retained for later detail views; not on
    /// the V1 face.
    double temperatureCelsius = 0.0;
    ValueAvailability temperatureAvailability = ValueAvailability::Absent;

    /// Disk read/write throughput (bytes/sec). Retained for later detail views;
    /// not on the V1 face.
    double readBytesPerSec = 0.0;
    ValueAvailability readAvailability = ValueAvailability::Absent;
    double writeBytesPerSec = 0.0;
    ValueAvailability writeAvailability = ValueAvailability::Absent;
};

}  // namespace darkspark::deck::instruments

#endif  // DARKSPARK_DECK_INSTRUMENTS_STORAGEINSTRUMENTMODEL_HPP
