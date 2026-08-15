// SPDX-License-Identifier: GPL-3.0-or-later
#include "deck/layout/DeckLayoutCollection.hpp"

namespace darkspark::deck::layout {

bool isValidCollection(const DeckLayoutCollection& collection) {
    if (collection.pages.empty()) {
        return false;
    }

    // Unique page ids.
    for (std::size_t i = 0; i < collection.pages.size(); ++i) {
        for (std::size_t j = i + 1; j < collection.pages.size(); ++j) {
            if (collection.pages[i].pageId == collection.pages[j].pageId) {
                return false;
            }
        }
    }

    // activePageId must refer to an existing page.
    bool activeFound = false;
    for (const DeckPageDefinition& page : collection.pages) {
        if (page.pageId == collection.activePageId) {
            activeFound = true;
            break;
        }
    }
    if (!activeFound) {
        return false;
    }

    // Each page's single-page layout must be individually valid. Empty pages
    // (no placements) are valid by isValidLayout.
    for (const DeckPageDefinition& page : collection.pages) {
        if (!isValidLayout(page.layout)) {
            return false;
        }
    }

    return true;
}

const DeckLayoutCollection& defaultCommandDeckCollection() {
    static const DeckLayoutCollection kCollection = [] {
        DeckLayoutCollection c;
        c.activePageId = 0;

        // Page 0 "System": the current Command Deck layout, unchanged.
        DeckPageDefinition system;
        system.pageId = 0;
        system.name = QStringLiteral("System");
        system.layout = defaultCommandDeckLayout();
        c.pages.push_back(system);

        // Page 1 "Controls": empty for now (a legitimate empty layout).
        DeckPageDefinition controls;
        controls.pageId = 1;
        controls.name = QStringLiteral("Controls");
        controls.layout = DeckLayout{/*pageId=*/1, {}};
        c.pages.push_back(controls);

        // Page 2 "Custom": empty for now.
        DeckPageDefinition custom;
        custom.pageId = 2;
        custom.name = QStringLiteral("Custom");
        custom.layout = DeckLayout{/*pageId=*/2, {}};
        c.pages.push_back(custom);

        return c;
    }();
    return kCollection;
}

}  // namespace darkspark::deck::layout
