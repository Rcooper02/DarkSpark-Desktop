// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_DECK_LAYOUT_DECKLAYOUT_HPP
#define DARKSPARK_DECK_LAYOUT_DECKLAYOUT_HPP

#include <vector>

#include "deck/instruments/InstrumentSizeMode.hpp"

// The Deck layout model: PURE DATA describing which widgets go where. It knows
// nothing about QWidget, telemetry, or instrument construction -- those live in
// the factory and the page. Keeping these types lightweight and
// serializable-looking (plain enums, ints, a vector of structs) is deliberate:
// the next batch adds JSON/QSettings persistence and a WidgetId<->string
// mapping without reworking this model.
namespace darkspark::deck::layout {

using instruments::InstrumentSizeMode;

/// Stable identity for each placeable Deck widget. An enum (not a string) so
/// the live layout code is compile-checked and switches over it are exhaustive,
/// exactly like MetricId. A WidgetId<->string mapping is deferred to the
/// persistence batch, where it belongs.
///
/// Unknown is a non-widget sentinel: it never appears in a valid layout and is
/// what a future string->id lookup returns for an unrecognised token. The
/// factory returns nullptr for it, so an unknown id fails gracefully.
enum class WidgetId {
    Unknown,
    Cpu,
    Gpu,
    Memory,
    Cooling,
    Storage,
    Network,
};

/// Which region of the Command Deck a widget occupies. The page is composed of
/// a dominant Primary region (the Large CPU) and a Secondary region (the Small
/// subsystem grid). Row/column below are interpreted WITHIN the placement's
/// region. This mirrors the current page structure exactly so Batch 1 is
/// visually identical; it is not a general layout engine.
enum class DeckRegion {
    Primary,
    Secondary,
};

/// One widget's placement: which widget, in which region, at which grid cell,
/// spanning how many cells, at what size, and whether it is placed at all.
/// Plain data -- copyable, comparable, trivially serialisable.
struct DeckWidgetPlacement {
    WidgetId id = WidgetId::Unknown;
    DeckRegion region = DeckRegion::Secondary;
    int row = 0;
    int column = 0;
    int rowSpan = 1;
    int columnSpan = 1;
    InstrumentSizeMode sizeMode = InstrumentSizeMode::Small;
    bool enabled = true;

    friend bool operator==(const DeckWidgetPlacement&,
                           const DeckWidgetPlacement&) = default;
};

/// One page's layout: an id plus its placements. A single page for Batch 1; a
/// collection of these is the natural next step for multiple pages. No QWidget,
/// no behaviour -- just the data.
struct DeckLayout {
    int pageId = 0;
    std::vector<DeckWidgetPlacement> placements;
};

/// Minimal Batch-1 invariants -- only what we actually need, no more:
///   * every enabled placement has non-negative row/column,
///   * spans >= 1,
///   * widget ids are unique among enabled placements (and not Unknown),
///   * enabled placements do not overlap (per region, accounting for spans).
/// Disabled placements are ignored entirely. Returns true when all hold.
[[nodiscard]] bool isValidLayout(const DeckLayout& layout);

/// The current, shipping Command Deck arrangement expressed as data. This IS the
/// visible layout today: CPU Large in Primary; GPU/Memory/Cooling on secondary
/// row 0 (cols 0/1/2) and Network/Storage on secondary row 1 (cols 0/1), with
/// secondary cell (1,2) intentionally empty. Building the page from this yields
/// a pixel-identical screen.
[[nodiscard]] const DeckLayout& defaultCommandDeckLayout();

}  // namespace darkspark::deck::layout

#endif  // DARKSPARK_DECK_LAYOUT_DECKLAYOUT_HPP
