// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_DECK_INSTRUMENTS_MEMORYINSTRUMENTMODELADAPTER_HPP
#define DARKSPARK_DECK_INSTRUMENTS_MEMORYINSTRUMENTMODELADAPTER_HPP

#include "deck/instruments/MemoryInstrumentModel.hpp"
#include "models/MetricSample.hpp"

namespace darkspark::deck::instruments {

/// Translates telemetry samples into a MemoryInstrumentModel.
///
/// Memory's own adapter, following the reference pattern established by
/// CpuInstrumentModelAdapter and mirrored by GpuInstrumentModelAdapter. It is
/// the single seam that knows BOTH the telemetry layer (MetricSample, MetricId,
/// MetricState) and the presentation layer (MemoryInstrumentModel,
/// ValueAvailability), so MemoryInstrument depends on neither services nor
/// /proc/meminfo.
///
///     MetricSample (from any provider)
///         -> MemoryInstrumentModelAdapter   (this type)
///         -> MemoryInstrumentModel
///         -> MemoryInstrument
///
/// Like the GPU adapter, it JOINS multiple independent samples: the memory
/// utilization percentage and the used/total byte figures arrive as separate
/// samples and are combined here. Available memory is DERIVED as total - used
/// (only when both are available), so no separate telemetry metric is needed.
/// Samples for other subsystems (CPU, GPU) are ignored, so a shared telemetry
/// stream does not disturb this instrument.
///
/// Data-quality maps to presentation availability without inventing health:
///   Fresh       -> Live
///   Stale       -> LastKnown (keeps the last numeric value)
///   Unavailable -> Absent    (no number; instrument shows a placeholder)
class MemoryInstrumentModelAdapter {
public:
    /// Apply one telemetry sample. Returns true if it changed the model (the
    /// sample was one this instrument cares about and altered a value or
    /// availability), false if it was irrelevant or a no-op.
    bool apply(const models::MetricSample& sample);

    [[nodiscard]] const MemoryInstrumentModel& model() const { return model_; }

    /// Reset to the initial all-Absent state (used when re-priming).
    void reset() { model_ = MemoryInstrumentModel{}; }

private:
    static ValueAvailability availabilityFor(models::MetricState state);

    /// Recompute derived availableBytes from the current used/total state.
    void updateDerivedAvailable();

    MemoryInstrumentModel model_{};
};

}  // namespace darkspark::deck::instruments

#endif  // DARKSPARK_DECK_INSTRUMENTS_MEMORYINSTRUMENTMODELADAPTER_HPP
