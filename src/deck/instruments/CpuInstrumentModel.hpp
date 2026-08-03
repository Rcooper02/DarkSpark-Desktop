// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_DECK_INSTRUMENTS_CPUINSTRUMENTMODEL_HPP
#define DARKSPARK_DECK_INSTRUMENTS_CPUINSTRUMENTMODEL_HPP

namespace darkspark::deck::instruments {

/// Presentation availability of a single value the instrument renders.
///
/// This is a PRESENTATION concept, intentionally distinct from the telemetry
/// layer's MetricState. The instrument must not depend on service or telemetry
/// types, so it never sees Fresh/Stale/Unavailable directly: an adapter
/// translates data-quality into this presentation availability. It carries no
/// health meaning -- it only says whether the instrument has a value to show,
/// has a last-known value that is no longer current, or has nothing to show.
enum class ValueAvailability {
    Live,       ///< a current value is present
    LastKnown,  ///< a previous value is shown but is no longer current
    Absent      ///< no value to show (render a neutral placeholder)
};

/// The values a CpuInstrument renders.
///
/// A plain presentation struct, deliberately decoupled from the telemetry layer:
/// the instrument paints what this struct says and knows nothing about
/// MetricSample, providers, sensor keys, or where the values came from. An
/// adapter maps telemetry samples onto this struct; the instrument does not
/// change when the data source does.
///
/// Availability is per-metric because utilization and package temperature can
/// have different fates in the same instant (for example a machine with no
/// exposed package-temperature sensor still has utilization). When a value is
/// Absent the numeric field is not meaningful and the instrument renders a
/// restrained placeholder rather than a fabricated number.
struct CpuInstrumentModel {
    /// CPU utilization, percent [0, 100]. The primary, dominant metric.
    double utilizationPercent = 0.0;
    ValueAvailability utilizationAvailability = ValueAvailability::Absent;

    /// CPU package temperature, degrees Celsius. The secondary metric.
    double temperatureCelsius = 0.0;
    ValueAvailability temperatureAvailability = ValueAvailability::Absent;
};

}  // namespace darkspark::deck::instruments

#endif  // DARKSPARK_DECK_INSTRUMENTS_CPUINSTRUMENTMODEL_HPP
