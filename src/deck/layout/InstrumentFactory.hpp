// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_DECK_LAYOUT_INSTRUMENTFACTORY_HPP
#define DARKSPARK_DECK_LAYOUT_INSTRUMENTFACTORY_HPP

#include "deck/instruments/InstrumentSizeMode.hpp"
#include "deck/layout/DeckLayout.hpp"

class QWidget;

// The single place that maps a WidgetId to a concrete instrument type. This is
// where the layout layer meets instrument construction: DeckLayout stays pure
// data, and this factory is the ONLY layout-side code that depends on the
// instrument classes. Telemetry wiring is NOT here -- it stays in Application,
// against the page's typed accessors.
namespace darkspark::deck::layout {

/// Construct the instrument for `id` at `size`, parented to `parent`.
///
/// Returns nullptr for WidgetId::Unknown or any id with no known instrument, so
/// an unknown/invalid id fails gracefully: the page simply skips it. The
/// returned widget is a QWidget*; callers that need the concrete type (for
/// telemetry wiring) recover it with qobject_cast.
[[nodiscard]] QWidget* createInstrument(WidgetId id, InstrumentSizeMode size,
                                        QWidget* parent);

}  // namespace darkspark::deck::layout

#endif  // DARKSPARK_DECK_LAYOUT_INSTRUMENTFACTORY_HPP
