// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_DECK_LAYOUT_DECKLAYOUTEDITS_HPP
#define DARKSPARK_DECK_LAYOUT_DECKLAYOUTEDITS_HPP

#include "deck/layout/DeckLayout.hpp"

// Pure-data editing operations on a DeckLayout: find a placement, test whether a
// widget can move to a cell, move it, and enable/disable it. These are the
// mechanics behind Edit Mode, kept entirely independent of QWidget so they are
// trivially unit-testable and reusable. They mutate a working-copy DeckLayout;
// the caller (the page) owns that copy and applies the result to real widgets.
//
// isValidLayout() remains the final gate: move/setWidgetEnabled apply a change
// only if the result stays valid, and the caller re-validates before Save.
namespace darkspark::deck::layout {

/// Return a pointer to the placement for `id`, or nullptr if none. Const and
/// non-const overloads; the pointer is into the layout's vector and is
/// invalidated by any operation that resizes placements (none here do).
[[nodiscard]] const DeckWidgetPlacement* findPlacement(const DeckLayout& layout,
                                                       WidgetId id);
[[nodiscard]] DeckWidgetPlacement* findPlacement(DeckLayout& layout,
                                                 WidgetId id);

/// Would placing `id` at (region, row, column) -- keeping its current span and
/// size -- yield a valid layout? Tests a COPY, so the real layout is untouched.
/// The widget's own current placement does not count as a self-overlap because
/// the move replaces it. Returns false if `id` is not present, if row/column is
/// negative, or if the result would be invalid (overlap, etc.).
[[nodiscard]] bool canPlace(const DeckLayout& layout, WidgetId id,
                            DeckRegion region, int row, int column);

/// Move `id` to (region, row, column), preserving its rowSpan, columnSpan, and
/// sizeMode. Applies only if canPlace() holds; returns true on success, false
/// (leaving the layout unchanged) otherwise.
bool moveWidget(DeckLayout& layout, WidgetId id, DeckRegion region, int row,
                int column);

/// Enable or disable `id`. Disabling always succeeds (a disabled placement is
/// ignored by validation and simply not shown). Enabling succeeds only if the
/// result stays valid (an enabled widget must not overlap another). Returns
/// true on success, false (layout unchanged) if `id` is absent or the result
/// would be invalid.
bool setWidgetEnabled(DeckLayout& layout, WidgetId id, bool enabled);

}  // namespace darkspark::deck::layout

#endif  // DARKSPARK_DECK_LAYOUT_DECKLAYOUTEDITS_HPP
