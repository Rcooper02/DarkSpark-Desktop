// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_DECK_INSTRUMENTS_INSTRUMENTRENDERMODEL_HPP
#define DARKSPARK_DECK_INSTRUMENTS_INSTRUMENTRENDERMODEL_HPP

#include <QColor>
#include <QString>

#include "deck/instruments/CpuInstrumentModel.hpp"  // for ValueAvailability
#include "deck/instruments/InstrumentSizeMode.hpp"

namespace darkspark::deck::instruments {

/// The two conduit accents an instrument renders with. Subsystem-agnostic: each
/// instrument supplies its own colours (CPU cyan, GPU a Forge-tinted variant,
/// etc.), and the renderer draws them without knowing which subsystem they came
/// from.
struct InstrumentAccents {
    QColor utilization;  ///< outer, primary conduit
    QColor temperature;  ///< inner, secondary conduit
};

/// The complete, subsystem-agnostic description of ONE instrument frame to
/// render. This is the boundary between a subsystem's own presentation state
/// (CpuInstrumentModel, GpuInstrumentModel, ...) and the shared DarkSpark
/// rendering language.
///
/// The renderer knows only this struct: two numeric readings with their
/// availability, a title, whether the instrument is a dormant shell, the size
/// mode, and the accents to draw with. It contains no CPU/GPU/subsystem
/// identity, no telemetry types, and no interpolation state -- each instrument
/// owns those and reduces them to this neutral description per paint.
/// One value the renderer paints. Subsystem- and unit-agnostic: the instrument
/// supplies pre-formatted text and its unit suffix, and the renderer draws them
/// verbatim. This single shape is used twice below (primary and secondary), so
/// there is exactly one contract for "a value on an instrument".
///
/// IMPORTANT boundary: `progress` is NOT telemetry. Telemetry reports truth
/// (the real reading lives in `text`); `progress` is VISUALIZATION STATE -- the
/// instrument's choice of how to fill a conduit [0, 1] for this value. An
/// instrument may derive it from a percentage, an adaptive observed range, a
/// hardware maximum, or nothing at all; the renderer neither knows nor cares
/// which. This separation (truth vs. visualization vs. painting) is a permanent
/// DarkSpark design rule and matters for Cooling, AI, Battery, Storage and Power
/// where the "fill" is a presentation choice rather than a fraction of a real
/// maximum. When `availability` is Absent the renderer shows a neutral "--"
/// placeholder instead of `text`.
struct RenderValue {
    // Field roles are an architectural guardrail -- keep these boundaries intact:
    //   text         : PRESENTATION   -- the instrument's formatted value string
    //   suffix       : PRESENTATION   -- the instrument's unit string
    //   progress     : VISUALIZATION  -- how the instrument chose to fill the
    //                                    conduit [0,1]; NOT telemetry, NOT a
    //                                    fraction of any real maximum
    //   availability : TELEMETRY STATE-- whether the underlying reading is Live,
    //                                    LastKnown, or Absent
    QString text;      ///< PRESENTATION: formatted value, e.g. "42", "1180", "12.4 / 32.0"
    QString suffix;    ///< PRESENTATION: unit, e.g. "%", " RPM", " GB", "\u00B0C"
    double progress = 0.0;  ///< VISUALIZATION only: [0,1] conduit fill, not telemetry
    ValueAvailability availability = ValueAvailability::Absent;  ///< TELEMETRY state
};

/// The complete, subsystem-agnostic description of ONE instrument frame to
/// render. This is the boundary between a subsystem's own presentation state
/// (CpuInstrumentModel, GpuInstrumentModel, ...) and the shared DarkSpark
/// rendering language.
///
/// The renderer knows only this struct: two RenderValues (primary and
/// secondary), a title, whether the instrument is a dormant shell, the size
/// mode, and the accents to draw with. It contains no CPU/GPU/subsystem
/// identity, NO UNIT KNOWLEDGE (never interprets %, RPM, Celsius, bytes, watts,
/// ...), no telemetry types, and no interpolation state -- each instrument owns
/// those and reduces them to this neutral description per paint. The renderer
/// simply paints text + suffix and fills conduits from progress.
struct InstrumentRenderModel {
    /// The dominant reading (outer conduit + large value).
    RenderValue primary;
    /// The subordinate reading (inner conduit + secondary line).
    RenderValue secondary;
    /// Whether to draw the inner (secondary) conduit at all. Instruments with a
    /// second ring set true (CPU/GPU temperature ring); a single-ring instrument
    /// sets false. The renderer acts on this flag, never on subsystem identity.
    bool hasSecondaryRing = true;

    QString title;                       ///< "CPU", "GPU", ...
    bool awaitingTelemetry = false;      ///< dormant shell => "Awaiting Telemetry"

    InstrumentSizeMode mode = InstrumentSizeMode::Large;
    InstrumentAccents accents;

    /// Energy intensity multiplier for the active conduits (future hover/touch
    /// growth). Instruments pass 1.0 today; the renderer honours it so the
    /// interaction states are a data change, not a renderer change.
    double glowStrength = 1.0;
};

}  // namespace darkspark::deck::instruments

#endif  // DARKSPARK_DECK_INSTRUMENTS_INSTRUMENTRENDERMODEL_HPP
