// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_DECK_INSTRUMENTS_INSTRUMENTRENDERER_HPP
#define DARKSPARK_DECK_INSTRUMENTS_INSTRUMENTRENDERER_HPP

#include <QRect>

#include "deck/instruments/InstrumentRenderModel.hpp"

class QPainter;

namespace darkspark::deck::instruments {

/// The shared DarkSpark instrument rendering language.
///
/// This is the SINGLE implementation of how a DarkSpark instrument looks:
/// recessed chamber, graduation ticks, segmented energy conduits (bloom,
/// fiber-optic cores, chamfered ends, dormant "ready" segments), and the
/// center information stack (title, dominant value, secondary line), composed
/// as the three back-to-front layers Structure / Energy / Information.
///
/// It is deliberately subsystem-agnostic: it renders an InstrumentRenderModel
/// and knows nothing about CPU, GPU, or any subsystem, nothing about telemetry,
/// and nothing about interpolation. CpuInstrument, GpuInstrument, and every
/// future subsystem instrument keep their OWN model, adapter, telemetry, size
/// policy, and interpolation state, and share only this drawing code -- so the
/// DarkSpark visual language lives in one place and evolves once for all.
///
/// Usage: an instrument's paintEvent builds an InstrumentRenderModel from its
/// current (interpolated) presentation state and calls paint() with its widget
/// rectangle. The renderer centres a square content box in that rectangle.
namespace InstrumentRenderer {

/// Render one instrument frame into `widgetRect` on `painter`. Centres a square
/// content box and draws the full three-layer composition for `model`.
void paint(QPainter& painter, const QRect& widgetRect,
           const InstrumentRenderModel& model);

}  // namespace InstrumentRenderer

}  // namespace darkspark::deck::instruments

#endif  // DARKSPARK_DECK_INSTRUMENTS_INSTRUMENTRENDERER_HPP
