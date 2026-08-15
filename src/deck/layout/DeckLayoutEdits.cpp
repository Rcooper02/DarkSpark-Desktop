// SPDX-License-Identifier: GPL-3.0-or-later
#include "deck/layout/DeckLayoutEdits.hpp"

namespace darkspark::deck::layout {

const DeckWidgetPlacement* findPlacement(const DeckLayout& layout, WidgetId id) {
    for (const DeckWidgetPlacement& p : layout.placements) {
        if (p.id == id) {
            return &p;
        }
    }
    return nullptr;
}

DeckWidgetPlacement* findPlacement(DeckLayout& layout, WidgetId id) {
    for (DeckWidgetPlacement& p : layout.placements) {
        if (p.id == id) {
            return &p;
        }
    }
    return nullptr;
}

bool canPlace(const DeckLayout& layout, WidgetId id, DeckRegion region, int row,
              int column) {
    if (row < 0 || column < 0) {
        return false;
    }
    const DeckWidgetPlacement* existing = findPlacement(layout, id);
    if (existing == nullptr) {
        return false;
    }
    // Batch 4 editing is region-local. Primary instruments must remain in the
    // Primary region and Secondary instruments must remain in Secondary. This
    // prevents a Large primary instrument from being constrained into a small
    // secondary cell and keeps the current edit surface reversible.
    if (region != existing->region) {
        return false;
    }
    // Test on a copy: move the widget's placement and check whole-layout
    // validity. Because we edit the widget's own entry (not add a second), there
    // is no self-overlap; isValidLayout catches overlaps with OTHER enabled
    // widgets, negative coords, and span violations.
    DeckLayout probe = layout;
    DeckWidgetPlacement* target = findPlacement(probe, id);
    if (target == nullptr) {
        return false;  // unreachable given existing != nullptr, but defensive
    }
    target->region = region;
    target->row = row;
    target->column = column;
    return isValidLayout(probe);
}

bool moveWidget(DeckLayout& layout, WidgetId id, DeckRegion region, int row,
                int column) {
    if (!canPlace(layout, id, region, row, column)) {
        return false;
    }
    DeckWidgetPlacement* target = findPlacement(layout, id);
    if (target == nullptr) {
        return false;
    }
    // Preserve rowSpan, columnSpan, and sizeMode; only the cell/region change.
    target->region = region;
    target->row = row;
    target->column = column;
    return true;
}

bool setWidgetEnabled(DeckLayout& layout, WidgetId id, bool enabled) {
    DeckWidgetPlacement* target = findPlacement(layout, id);
    if (target == nullptr) {
        return false;
    }
    if (target->enabled == enabled) {
        return true;  // no-op, already in the requested state
    }
    const bool previous = target->enabled;
    target->enabled = enabled;
    // Disabling can never invalidate (fewer enabled widgets). Enabling might
    // reintroduce an overlap; gate it on validity and roll back if it fails.
    if (enabled && !isValidLayout(layout)) {
        target->enabled = previous;
        return false;
    }
    return true;
}

}  // namespace darkspark::deck::layout
