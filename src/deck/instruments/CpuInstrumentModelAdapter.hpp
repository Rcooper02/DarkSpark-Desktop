// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_DECK_INSTRUMENTS_CPUINSTRUMENTMODELADAPTER_HPP
#define DARKSPARK_DECK_INSTRUMENTS_CPUINSTRUMENTMODELADAPTER_HPP

#include "deck/instruments/CpuInstrumentModel.hpp"
#include "models/MetricSample.hpp"

namespace darkspark::deck::instruments {

/// Translates telemetry samples into a CpuInstrumentModel.
///
/// REFERENCE PATTERN. This adapter is intentionally the template that future
/// subsystem adapters (GPU, Memory, Cooling, Storage, Network, ...) should
/// follow -- independently, each as its own small class. It is deliberately NOT
/// a base class or shared framework: there is no InstrumentAdapterBase and none
/// is wanted. The pattern is the reusable thing, not the code. A subsystem
/// adapter following this pattern should:
///
///   1. Own a plain presentation model (its instrument's <X>InstrumentModel),
///      and nothing else -- no services, no Qt, no filesystem, no history.
///   2. Expose a single apply(const MetricSample&) that selects only the
///      metrics and sensor keys its instrument shows, ignoring the rest, and
///      returns whether the model changed.
///   3. Map data-quality (Fresh/Stale/Unavailable) to presentation availability
///      (Live/LastKnown/Absent) and NOTHING else -- no health, no thresholds,
///      no policy. Presentation availability is the only interpretation an
///      adapter performs.
///   4. Sanitize values that have an obvious domain (e.g. clamp a percentage to
///      [0,100]) so the instrument never has to defend against bad input.
///   5. Stay pure and Qt-free, so it is fully unit-testable without a display or
///      live services.
///
/// This adapter is the single seam that knows BOTH worlds: the telemetry layer
/// (MetricSample, MetricId, MetricState, sensor keys) and the presentation layer
/// (CpuInstrumentModel, ValueAvailability). It exists so that CpuInstrument
/// itself depends on neither services nor filesystem discovery -- the instrument
/// only ever sees a CpuInstrumentModel.
///
///     MetricSample (from any provider)
///         -> CpuInstrumentModelAdapter   (this type)
///         -> CpuInstrumentModel
///         -> CpuInstrument
///
/// It selects the CPU utilization metric and the package temperature sensor
/// specifically; other samples (memory, CCD temperatures) are ignored, so a
/// provider emitting many samples does not disturb the two values this
/// instrument shows.
///
/// Data-quality maps to presentation availability without inventing health:
///   Fresh       -> Live
///   Stale       -> LastKnown (keeps the last numeric value)
///   Unavailable -> Absent    (no number; instrument shows a placeholder)
class CpuInstrumentModelAdapter {
public:
    /// The stable sensor key identifying the package temperature among CPU
    /// temperature samples. CCD samples carry other keys and are ignored here.
    static constexpr const char* kPackageKey = "package";

    /// Apply one telemetry sample. Returns true if it changed the model (i.e.
    /// the sample was one this instrument cares about and altered a value or
    /// availability), false if it was irrelevant or a no-op.
    bool apply(const models::MetricSample& sample);

    [[nodiscard]] const CpuInstrumentModel& model() const { return model_; }

    /// Reset to the initial all-Absent state (used when re-priming).
    void reset() { model_ = CpuInstrumentModel{}; }

private:
    static ValueAvailability availabilityFor(models::MetricState state);

    CpuInstrumentModel model_{};
};

}  // namespace darkspark::deck::instruments

#endif  // DARKSPARK_DECK_INSTRUMENTS_CPUINSTRUMENTMODELADAPTER_HPP
