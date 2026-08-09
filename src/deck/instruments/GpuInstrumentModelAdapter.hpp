// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_DECK_INSTRUMENTS_GPUINSTRUMENTMODELADAPTER_HPP
#define DARKSPARK_DECK_INSTRUMENTS_GPUINSTRUMENTMODELADAPTER_HPP

#include "deck/instruments/GpuInstrumentModel.hpp"
#include "models/MetricSample.hpp"

namespace darkspark::deck::instruments {

/// Translates telemetry samples into a GpuInstrumentModel.
///
/// GPU's own adapter, following the reference pattern established by
/// CpuInstrumentModelAdapter (see its header for the five-point template). It is
/// the single seam that knows BOTH the telemetry layer (MetricSample, MetricId,
/// MetricState, sensor keys) and the presentation layer (GpuInstrumentModel,
/// ValueAvailability), so GpuInstrument depends on neither services nor sysfs.
///
///     MetricSample (from any provider)
///         -> GpuInstrumentModelAdapter   (this type)
///         -> GpuInstrumentModel
///         -> GpuInstrument
///
/// It selects the GPU utilization metric and the primary GPU temperature sensor
/// specifically; other samples (CPU, memory, and any non-primary GPU
/// temperature key) are ignored, so a shared telemetry stream does not disturb
/// the two values this instrument shows.
///
/// Data-quality maps to presentation availability without inventing health:
///   Fresh       -> Live
///   Stale       -> LastKnown (keeps the last numeric value)
///   Unavailable -> Absent    (no number; instrument shows a placeholder)
class GpuInstrumentModelAdapter {
public:
    /// The stable sensor key identifying the primary GPU temperature. The
    /// thermal provider publishes its selected physical sensor (junction or
    /// edge) under this single stable key, so the adapter is independent of
    /// which label the hardware exposed.
    static constexpr const char* kPrimaryTempKey = "gpu";
    /// VRAM samples (MemoryUsedBytes/MemoryTotalBytes) carry this key so the GPU
    /// adapter consumes GPU VRAM but never SYSTEM RAM (which is emitted under the
    /// same MetricIds with NO key by MemoryTelemetryService).
    static constexpr const char* kVramKey = "gpu-vram";

    /// Apply one telemetry sample. Returns true if it changed the model (the
    /// sample was one this instrument cares about and altered a value or
    /// availability), false if it was irrelevant or a no-op.
    bool apply(const models::MetricSample& sample);

    [[nodiscard]] const GpuInstrumentModel& model() const { return model_; }

    /// Reset to the initial all-Absent state (used when re-priming).
    void reset() { model_ = GpuInstrumentModel{}; }

private:
    static ValueAvailability availabilityFor(models::MetricState state);

    GpuInstrumentModel model_{};
};

}  // namespace darkspark::deck::instruments

#endif  // DARKSPARK_DECK_INSTRUMENTS_GPUINSTRUMENTMODELADAPTER_HPP
