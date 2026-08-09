// SPDX-License-Identifier: GPL-3.0-or-later
#include "deck/layout/DeckLayout.hpp"

namespace darkspark::deck::layout {

bool isValidLayout(const DeckLayout& layout) {
    // Collect enabled placements; disabled ones are ignored entirely.
    struct Cell {
        DeckRegion region;
        int row;
        int column;
        int rowSpan;
        int columnSpan;
    };
    std::vector<Cell> cells;
    cells.reserve(layout.placements.size());

    for (const DeckWidgetPlacement& p : layout.placements) {
        if (!p.enabled) {
            continue;
        }
        // A placed widget must be a real widget, not the Unknown sentinel.
        if (p.id == WidgetId::Unknown) {
            return false;
        }
        // Non-negative grid origin; spans at least one cell.
        if (p.row < 0 || p.column < 0 || p.rowSpan < 1 || p.columnSpan < 1) {
            return false;
        }
        cells.push_back(Cell{p.region, p.row, p.column, p.rowSpan, p.columnSpan});
    }

    // Unique widget ids among enabled placements.
    for (std::size_t i = 0; i < layout.placements.size(); ++i) {
        if (!layout.placements[i].enabled) {
            continue;
        }
        for (std::size_t j = i + 1; j < layout.placements.size(); ++j) {
            if (!layout.placements[j].enabled) {
                continue;
            }
            if (layout.placements[i].id == layout.placements[j].id) {
                return false;
            }
        }
    }

    // No two enabled placements overlap within the same region. Overlap is
    // rectangle intersection over [row, row+rowSpan) x [col, col+colSpan).
    // Different regions never conflict (they are laid out independently).
    for (std::size_t i = 0; i < cells.size(); ++i) {
        for (std::size_t j = i + 1; j < cells.size(); ++j) {
            const Cell& a = cells[i];
            const Cell& b = cells[j];
            if (a.region != b.region) {
                continue;
            }
            const bool rowsOverlap = a.row < b.row + b.rowSpan
                                     && b.row < a.row + a.rowSpan;
            const bool colsOverlap = a.column < b.column + b.columnSpan
                                     && b.column < a.column + a.columnSpan;
            if (rowsOverlap && colsOverlap) {
                return false;
            }
        }
    }

    return true;
}

const DeckLayout& defaultCommandDeckLayout() {
    // The current shipping arrangement, as data. Positions match the page today
    // exactly: CPU Large in the Primary region; the five Small subsystem
    // instruments in the Secondary grid at their current cells, with secondary
    // (1,2) intentionally empty (no placement -> no widget there).
    static const DeckLayout kLayout = {
        /*pageId=*/0,
        {
            {WidgetId::Cpu, DeckRegion::Primary, 0, 0, 1, 1,
             InstrumentSizeMode::Large, true},
            {WidgetId::Gpu, DeckRegion::Secondary, 0, 0, 1, 1,
             InstrumentSizeMode::Small, true},
            {WidgetId::Memory, DeckRegion::Secondary, 0, 1, 1, 1,
             InstrumentSizeMode::Small, true},
            {WidgetId::Cooling, DeckRegion::Secondary, 0, 2, 1, 1,
             InstrumentSizeMode::Small, true},
            {WidgetId::Network, DeckRegion::Secondary, 1, 0, 1, 1,
             InstrumentSizeMode::Small, true},
            {WidgetId::Storage, DeckRegion::Secondary, 1, 1, 1, 1,
             InstrumentSizeMode::Small, true},
            // Secondary cell (1,2) is intentionally not placed: reserved empty
            // capacity, exactly as today.
        },
    };
    return kLayout;
}

}  // namespace darkspark::deck::layout
