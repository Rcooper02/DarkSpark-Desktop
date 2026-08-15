// SPDX-License-Identifier: GPL-3.0-or-later
//
// Tests for the multi-page collection model: the default three-page collection
// is valid, page ids are unique, activePageId refers to a real page, and empty
// pages (Controls/Custom) are valid. Qt-light (uses QString via the model);
// pure data otherwise. Own main().

#include <cstdio>

#include "deck/layout/DeckLayout.hpp"
#include "deck/layout/DeckLayoutCollection.hpp"

using namespace darkspark::deck::layout;

namespace {
int g_failures = 0;
void reportFail(const char* e, const char* f, int l) {
    std::fprintf(stderr, "FAIL: %s  (%s:%d)\n", e, f, l);
    ++g_failures;
}
#define CHECK(c) do { if (!(c)) reportFail(#c, __FILE__, __LINE__); } while (0)

const DeckPageDefinition* findPage(const DeckLayoutCollection& c, int pageId) {
    for (const DeckPageDefinition& p : c.pages) {
        if (p.pageId == pageId) return &p;
    }
    return nullptr;
}

void test_default_three_pages_valid() {
    const DeckLayoutCollection& c = defaultCommandDeckCollection();
    CHECK(isValidCollection(c));
    CHECK(c.pages.size() == 3);
    CHECK(c.activePageId == 0);
}

void test_default_page_names_and_ids() {
    const DeckLayoutCollection& c = defaultCommandDeckCollection();
    const auto* system = findPage(c, 0);
    const auto* controls = findPage(c, 1);
    const auto* custom = findPage(c, 2);
    CHECK(system != nullptr && system->name == QStringLiteral("System"));
    CHECK(controls != nullptr && controls->name == QStringLiteral("Controls"));
    CHECK(custom != nullptr && custom->name == QStringLiteral("Custom"));
    // System has the six instruments; Controls/Custom are empty.
    CHECK(system->layout.placements.size() == 6);
    CHECK(controls->layout.placements.empty());
    CHECK(custom->layout.placements.empty());
}

void test_unique_page_ids() {
    DeckLayoutCollection c = defaultCommandDeckCollection();
    // Force a duplicate id -> invalid.
    c.pages[1].pageId = 0;
    CHECK(!isValidCollection(c));
}

void test_active_page_must_exist() {
    DeckLayoutCollection c = defaultCommandDeckCollection();
    c.activePageId = 99;  // no such page
    CHECK(!isValidCollection(c));
}

void test_empty_page_is_valid() {
    // A collection of a single empty page is valid.
    DeckLayoutCollection c;
    c.activePageId = 7;
    DeckPageDefinition p;
    p.pageId = 7;
    p.name = QStringLiteral("Empty");
    p.layout = DeckLayout{7, {}};
    c.pages.push_back(p);
    CHECK(isValidCollection(c));
}

void test_empty_collection_invalid() {
    DeckLayoutCollection c;  // no pages
    CHECK(!isValidCollection(c));
}

void test_invalid_page_layout_invalidates_collection() {
    DeckLayoutCollection c = defaultCommandDeckCollection();
    // Corrupt the System page: negative row on a placement.
    c.pages[0].layout.placements[0].row = -1;
    CHECK(!isValidCollection(c));
}

}  // namespace

int main() {
    test_default_three_pages_valid();
    test_default_page_names_and_ids();
    test_unique_page_ids();
    test_active_page_must_exist();
    test_empty_page_is_valid();
    test_empty_collection_invalid();
    test_invalid_page_layout_invalidates_collection();
    if (g_failures == 0) {
        std::puts("All DeckLayoutCollection tests passed.");
        return 0;
    }
    std::fprintf(stderr, "%d DeckLayoutCollection check(s) failed.\n",
                 g_failures);
    return 1;
}
