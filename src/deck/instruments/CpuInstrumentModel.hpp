// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_DECK_INSTRUMENTS_CPUINSTRUMENTMODEL_HPP
#define DARKSPARK_DECK_INSTRUMENTS_CPUINSTRUMENTMODEL_HPP

namespace darkspark::deck::instruments {

/// The values a CpuInstrument renders.
///
/// This is a plain presentation struct, deliberately decoupled from the
/// telemetry layer: the instrument paints numbers and knows nothing about
/// MetricSample, providers, or where the values came from. In the prototype it
/// holds fixed mock values. When live data is bound later, a thin adapter will
/// map a telemetry sample onto this struct and the instrument will not change.
struct CpuInstrumentModel {
    /// CPU utilization, percent [0, 100]. The primary, dominant metric.
    double utilizationPercent = 42.0;

    /// CPU package temperature, degrees Celsius. The secondary metric.
    double temperatureCelsius = 61.0;
};

}  // namespace darkspark::deck::instruments

#endif  // DARKSPARK_DECK_INSTRUMENTS_CPUINSTRUMENTMODEL_HPP
