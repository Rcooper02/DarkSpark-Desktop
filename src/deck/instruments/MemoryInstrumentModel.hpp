// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_DECK_INSTRUMENTS_MEMORYINSTRUMENTMODEL_HPP
#define DARKSPARK_DECK_INSTRUMENTS_MEMORYINSTRUMENTMODEL_HPP

#include "deck/instruments/CpuInstrumentModel.hpp"  // for ValueAvailability

namespace darkspark::deck::instruments {

/// The values a MemoryInstrument renders.
///
/// A plain presentation struct, memory's own type -- deliberately NOT reusing
/// Cpu/GpuInstrumentModel, so memory can carry its own byte figures without
/// touching the others. It shares only ValueAvailability, a presentation
/// primitive.
///
/// The instrument paints what this struct says and knows nothing about
/// MetricSample, providers, or /proc/meminfo. An adapter maps telemetry samples
/// onto this struct. Availability is per-value because the utilization
/// percentage and the byte figures arrive as independent samples and can have
/// different fates in the same instant. When a value is Absent its numeric field
/// is not meaningful and the instrument renders a restrained placeholder rather
/// than a fabricated number.
struct MemoryInstrumentModel {
    /// Memory utilization, percent [0, 100]. The primary, dominant metric.
    double utilizationPercent = 0.0;
    ValueAvailability utilizationAvailability = ValueAvailability::Absent;

    /// Used memory in bytes. Part of the secondary "used / total" line.
    double usedBytes = 0.0;
    ValueAvailability usedAvailability = ValueAvailability::Absent;

    /// Total memory in bytes. Part of the secondary "used / total" line.
    double totalBytes = 0.0;
    ValueAvailability totalAvailability = ValueAvailability::Absent;

    /// Available memory in bytes, derived by the adapter as total - used. Kept
    /// in the model because it is genuinely useful to a reader and derivable
    /// from trustworthy values without a separate telemetry metric. Available
    /// only when both used and total are available.
    double availableBytes = 0.0;
    ValueAvailability availableAvailability = ValueAvailability::Absent;
};

}  // namespace darkspark::deck::instruments

#endif  // DARKSPARK_DECK_INSTRUMENTS_MEMORYINSTRUMENTMODEL_HPP
