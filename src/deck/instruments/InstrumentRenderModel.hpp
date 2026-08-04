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

    double temperatureCelsius = 0.0;
    ValueAvailability temperatureAvailability = ValueAvailability::Absent;

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
