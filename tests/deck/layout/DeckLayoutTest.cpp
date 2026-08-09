// SPDX-License-Identifier: GPL-3.0-or-later
//
// Tests for the pure-data Deck layout model: the default layout is valid and
// reproduces the current arrangement, the minimal invariants hold, and invalid
// layouts are rejected. Qt-free and display-free -- pure data.

#include <cstdio>

#include "deck/layout/DeckLayout.hpp"

using namespace darkspark::deck::layout;

namespace {
int g_failures = 0;
void reportFail(const char* e, const char* f, int l) {
    std::fprintf(stderr, "FAIL: %s  (%s:%d)\n", e, f, l);
    ++g_failures;
}
#define CHECK(c) do { if (!(c)) reportFail(#c, __FILE__, __LINE__); } while (0)

// Find the placement for a widget id in a layout (enabled or not).
const DeckWidgetPlacement* find(const DeckLayout& l, WidgetId id) {
    for (const DeckWidgetPlacement& p : l.placements) {
        if (p.id == id) return &p;
    }
    return nullptr;
}

void test_default_is_valid() {
    CHECK(isValidLayout(defaultCommandDeckLayout()));
}

void test_default_matches_current_arrangement() {
    const DeckLayout& l = defaultCommandDeckLayout();
    // Exactly the six known widgets, all enabled.
    CHECK(l.placements.size() == 6);
    // CPU: Primary region, Large.
    const auto* cpu = find(l, WidgetId::Cpu);
    CHECK(cpu != nullptr && cpu->region == DeckRegion::Primary
          && cpu->sizeMode == InstrumentSizeMode::Large && cpu->enabled);
    // Secondary row 0: GPU(0,0), Memory(0,1), Cooling(0,2).
    const auto* gpu = find(l, WidgetId::Gpu);
    const auto* mem = find(l, WidgetId::Memory);
    const auto* cool = find(l, WidgetId::Cooling);
    CHECK(gpu && gpu->region == DeckRegion::Secondary && gpu->row == 0
          && gpu->column == 0 && gpu->sizeMode == InstrumentSizeMode::Small);
    CHECK(mem && mem->row == 0 && mem->column == 1);
    CHECK(cool && cool->row == 0 && cool->column == 2);
    // Secondary row 1: Network(1,0), Storage(1,1). Cell (1,2) empty.
    const auto* net = find(l, WidgetId::Network);
    const auto* sto = find(l, WidgetId::Storage);
    CHECK(net && net->row == 1 && net->column == 0);
    CHECK(sto && sto->row == 1 && sto->column == 1);
    // Nothing occupies secondary (1,2).
    for (const DeckWidgetPlacement& p : l.placements) {
        const bool atEmptyCell = p.region == DeckRegion::Secondary
                                 && p.row == 1 && p.column == 2;
        CHECK(!atEmptyCell);
    }
}

void test_negative_origin_rejected() {
    DeckLayout l{0, {{WidgetId::Gpu, DeckRegion::Secondary, -1, 0, 1, 1,
                      InstrumentSizeMode::Small, true}}};
    CHECK(!isValidLayout(l));
    DeckLayout l2{0, {{WidgetId::Gpu, DeckRegion::Secondary, 0, -2, 1, 1,
                       InstrumentSizeMode::Small, true}}};
    CHECK(!isValidLayout(l2));
}

void test_zero_span_rejected() {
    DeckLayout l{0, {{WidgetId::Gpu, DeckRegion::Secondary, 0, 0, 0, 1,
                      InstrumentSizeMode::Small, true}}};
    CHECK(!isValidLayout(l));
    DeckLayout l2{0, {{WidgetId::Gpu, DeckRegion::Secondary, 0, 0, 1, 0,
                       InstrumentSizeMode::Small, true}}};
    CHECK(!isValidLayout(l2));
}

void test_duplicate_id_rejected() {
    DeckLayout l{0, {
        {WidgetId::Gpu, DeckRegion::Secondary, 0, 0, 1, 1,
         InstrumentSizeMode::Small, true},
        {WidgetId::Gpu, DeckRegion::Secondary, 0, 1, 1, 1,
         InstrumentSizeMode::Small, true},
    }};
    CHECK(!isValidLayout(l));
}

void test_unknown_id_rejected_when_enabled() {
    DeckLayout l{0, {{WidgetId::Unknown, DeckRegion::Secondary, 0, 0, 1, 1,
                      InstrumentSizeMode::Small, true}}};
    CHECK(!isValidLayout(l));
}

void test_overlap_rejected() {
    // Two enabled widgets on the same cell in the same region overlap.
    DeckLayout l{0, {
        {WidgetId::Gpu, DeckRegion::Secondary, 0, 0, 1, 1,
         InstrumentSizeMode::Small, true},
        {WidgetId::Memory, DeckRegion::Secondary, 0, 0, 1, 1,
         InstrumentSizeMode::Small, true},
    }};
    CHECK(!isValidLayout(l));
    // Spanning overlap: a 1x2 GPU at (0,0) collides with Memory at (0,1).
    DeckLayout span{0, {
        {WidgetId::Gpu, DeckRegion::Secondary, 0, 0, 1, 2,
         InstrumentSizeMode::Small, true},
        {WidgetId::Memory, DeckRegion::Secondary, 0, 1, 1, 1,
         InstrumentSizeMode::Small, true},
    }};
    CHECK(!isValidLayout(span));
}

void test_same_cell_different_region_ok() {
    // The same (row,col) in different regions is NOT an overlap.
    DeckLayout l{0, {
        {WidgetId::Cpu, DeckRegion::Primary, 0, 0, 1, 1,
         InstrumentSizeMode::Large, true},
        {WidgetId::Gpu, DeckRegion::Secondary, 0, 0, 1, 1,
         InstrumentSizeMode::Small, true},
    }};
    CHECK(isValidLayout(l));
}

void test_disabled_placement_ignored() {
    // A disabled placement is ignored: overlaps and duplicate ids with a
    // disabled entry do not invalidate the layout.
    DeckLayout l{0, {
        {WidgetId::Gpu, DeckRegion::Secondary, 0, 0, 1, 1,
         InstrumentSizeMode::Small, true},
        {WidgetId::Gpu, DeckRegion::Secondary, 0, 0, 1, 1,
         InstrumentSizeMode::Small, false},  // disabled duplicate/overlap
    }};
    CHECK(isValidLayout(l));
    // A disabled Unknown is also fine (never placed).
    DeckLayout u{0, {{WidgetId::Unknown, DeckRegion::Secondary, 0, 0, 1, 1,
                      InstrumentSizeMode::Small, false}}};
    CHECK(isValidLayout(u));
}

}  // namespace

int main() {
    test_default_is_valid();
    test_default_matches_current_arrangement();
    test_negative_origin_rejected();
    test_zero_span_rejected();
    test_duplicate_id_rejected();
    test_unknown_id_rejected_when_enabled();
    test_overlap_rejected();
    test_same_cell_different_region_ok();
    test_disabled_placement_ignored();
    if (g_failures == 0) {
        std::puts("All DeckLayout tests passed.");
        return 0;
    }
    std::fprintf(stderr, "%d DeckLayout check(s) failed.\n", g_failures);
    return 1;
}
