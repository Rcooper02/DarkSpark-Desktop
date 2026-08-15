// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_DECK_LAYOUT_DECKLAYOUTCOLLECTION_HPP
#define DARKSPARK_DECK_LAYOUT_DECKLAYOUTCOLLECTION_HPP

#include <vector>

#include <QString>

#include "deck/layout/DeckLayout.hpp"

// The multi-page collection model. DeckLayout remains the description of ONE
// page; this collection composes several named pages plus the active-page
// selection. Pure data, like DeckLayout: no QWidget, no persistence, no
// behaviour -- serialization lives in the serializer, file I/O in the service.
namespace darkspark::deck::layout {

/// One page in the deck: a stable id, a display name, and its single-page
/// layout. pageId here is authoritative for the collection (DeckLayout's own
/// pageId field is left as-is for backward compatibility but not relied upon).
struct DeckPageDefinition {
    int pageId = 0;
    QString name;
    DeckLayout layout;

    friend bool operator==(const DeckPageDefinition&,
                           const DeckPageDefinition&) = default;
};

/// A collection of pages plus which one is active. This is what persistence
/// stores (schema version 2) and what the deck builds its PageManager from.
struct DeckLayoutCollection {
    int activePageId = 0;
    std::vector<DeckPageDefinition> pages;
};

/// Collection invariants (page-level; each page's geometry is checked by
/// isValidLayout):
///   * at least one page,
///   * page ids are unique,
///   * activePageId refers to an existing page,
///   * every page's layout is individually valid (isValidLayout).
/// Empty pages (no placements) are valid -- Controls/Custom rely on this.
[[nodiscard]] bool isValidCollection(const DeckLayoutCollection& collection);

/// The compiled default three-page collection:
///   * pageId 0 "System"   -- the current Command Deck layout (unchanged),
///   * pageId 1 "Controls" -- empty,
///   * pageId 2 "Custom"   -- empty,
/// activePageId 0. Building the deck from this yields today's System page plus
/// two empty pages to navigate to.
[[nodiscard]] const DeckLayoutCollection& defaultCommandDeckCollection();

}  // namespace darkspark::deck::layout

#endif  // DARKSPARK_DECK_LAYOUT_DECKLAYOUTCOLLECTION_HPP
