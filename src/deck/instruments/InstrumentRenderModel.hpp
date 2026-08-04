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
struct InstrumentRenderModel {
    double utilizationPercent = 0.0;
    ValueAvailability utilizationAvailability = ValueAvailability::Absent;

    /// The secondary reading, presented neutrally. The renderer draws
    /// `secondaryText` verbatim as the subordinate line and does not know what
    /// it means: each instrument formats its own ("64\u00B0C" for CPU/GPU
    /// temperature, "12.4 / 32.0 GB" for memory, etc.), including any unit. When
    /// `secondaryAvailability` is Absent the renderer shows a neutral "--"
    /// placeholder instead of the text, so no instrument has to encode absence
    /// formatting. `secondaryValue` in [0, 1] drives the inner conduit fill; it
    /// is a pure ratio with no unit, so the renderer stays subsystem-agnostic.
    QString secondaryText;
    ValueAvailability secondaryAvailability = ValueAvailability::Absent;
    /// Inner-conduit fill fraction [0, 1] for the secondary reading. Instruments
    /// map their domain onto this ratio (CPU/GPU map a temperature range; memory
    /// maps used/total). 0 when there is no meaningful secondary ring.
    double secondaryValue = 0.0;
    /// Whether to draw the inner (secondary) conduit at all. CPU/GPU set true
    /// (temperature ring); an instrument with no second ring sets false.
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
