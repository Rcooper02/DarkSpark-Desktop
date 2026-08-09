// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_DECK_INSTRUMENTS_GPUINSTRUMENTMODEL_HPP
#define DARKSPARK_DECK_INSTRUMENTS_GPUINSTRUMENTMODEL_HPP

#include "deck/instruments/CpuInstrumentModel.hpp"  // for ValueAvailability

namespace darkspark::deck::instruments {

/// The values a GpuInstrument renders.
///
/// A plain presentation struct, GPU's own type -- deliberately NOT reusing
/// CpuInstrumentModel, so GPU can diverge (add board power, fan, memory-use,
/// clocks) without touching CPU. It shares only ValueAvailability, which is a
/// presentation primitive, not a CPU concept.
///
/// The instrument paints what this struct says and knows nothing about
/// MetricSample, providers, sysfs, or where the values came from. An adapter
/// maps telemetry samples onto this struct. Availability is per-metric because
/// utilization and temperature can have different fates in the same instant
/// (a card may expose utilization but no readable temperature sensor). When a
/// value is Absent the numeric field is not meaningful and the instrument
/// renders a restrained placeholder rather than a fabricated number.
struct GpuInstrumentModel {
    /// GPU utilization, percent [0, 100]. The primary, dominant metric.
    double utilizationPercent = 0.0;
    ValueAvailability utilizationAvailability = ValueAvailability::Absent;

    /// GPU temperature, degrees Celsius. The secondary metric.
    double temperatureCelsius = 0.0;
    ValueAvailability temperatureAvailability = ValueAvailability::Absent;

    /// GPU VRAM used / total, in bytes. The tertiary metric, shown as a third
    /// caption line ("VRAM x.x / y.y GB"). Availability is shared: both figures
    /// come from the same reading, so one flag governs the pair.
    double vramUsedBytes = 0.0;
    double vramTotalBytes = 0.0;
    ValueAvailability vramAvailability = ValueAvailability::Absent;
};

}  // namespace darkspark::deck::instruments

#endif  // DARKSPARK_DECK_INSTRUMENTS_GPUINSTRUMENTMODEL_HPP
