// SPDX-License-Identifier: GPL-3.0-or-later
//
// Tests for the pure-data layout edit operations. Qt-free, own main().

#include <cstdio>

#include "deck/layout/DeckLayout.hpp"
#include "deck/layout/DeckLayoutEdits.hpp"

using namespace darkspark::deck::layout;

namespace {
int g_failures = 0;
void reportFail(const char* e, const char* f, int l) {
    std::fprintf(stderr, "FAIL: %s  (%s:%d)\n", e, f, l);
    ++g_failures;
}
#define CHECK(c) do { if (!(c)) reportFail(#c, __FILE__, __LINE__); } while (0)

void test_find_existing_placement() {
    const DeckLayout L = defaultCommandDeckLayout();
    CHECK(findPlacement(L, WidgetId::Gpu) != nullptr);
    CHECK(findPlacement(L, WidgetId::Cpu) != nullptr);
    CHECK(findPlacement(L, WidgetId::Unknown) == nullptr);
}

void test_move_to_valid_empty_cell() {
    DeckLayout L = defaultCommandDeckLayout();
    // (1,2) is the intentionally-empty secondary cell.
    CHECK(canPlace(L, WidgetId::Gpu, DeckRegion::Secondary, 1, 2));
    CHECK(moveWidget(L, WidgetId::Gpu, DeckRegion::Secondary, 1, 2));
    const DeckWidgetPlacement* g = findPlacement(L, WidgetId::Gpu);
    CHECK(g != nullptr && g->row == 1 && g->column == 2);
}

void test_reject_overlap() {
    DeckLayout L = defaultCommandDeckLayout();
    // Memory occupies secondary (0,1); moving GPU there must be rejected.
    CHECK(!canPlace(L, WidgetId::Gpu, DeckRegion::Secondary, 0, 1));
    CHECK(!moveWidget(L, WidgetId::Gpu, DeckRegion::Secondary, 0, 1));
    // GPU is unchanged at its original (0,0).
    const DeckWidgetPlacement* g = findPlacement(L, WidgetId::Gpu);
    CHECK(g != nullptr && g->row == 0 && g->column == 0);
}

void test_reject_negative() {
    DeckLayout L = defaultCommandDeckLayout();
    CHECK(!canPlace(L, WidgetId::Gpu, DeckRegion::Secondary, -1, 0));
    CHECK(!canPlace(L, WidgetId::Gpu, DeckRegion::Secondary, 0, -1));
    CHECK(!moveWidget(L, WidgetId::Gpu, DeckRegion::Secondary, -1, 0));
    const DeckWidgetPlacement* g = findPlacement(L, WidgetId::Gpu);
    CHECK(g != nullptr && g->row == 0 && g->column == 0);  // unchanged
}

void test_reject_cross_region_moves() {
    DeckLayout L = defaultCommandDeckLayout();

    const DeckWidgetPlacement* cpuBefore = findPlacement(L, WidgetId::Cpu);
    CHECK(cpuBefore != nullptr);
    CHECK(cpuBefore->region == DeckRegion::Primary);
    CHECK(!canPlace(L, WidgetId::Cpu, DeckRegion::Secondary, 1, 2));
    CHECK(!moveWidget(L, WidgetId::Cpu, DeckRegion::Secondary, 1, 2));
    const DeckWidgetPlacement* cpuAfter = findPlacement(L, WidgetId::Cpu);
    CHECK(cpuAfter != nullptr);
    CHECK(cpuAfter->region == DeckRegion::Primary);
    CHECK(cpuAfter->row == cpuBefore->row && cpuAfter->column == cpuBefore->column);

    const DeckWidgetPlacement* gpuBefore = findPlacement(L, WidgetId::Gpu);
    CHECK(gpuBefore != nullptr);
    CHECK(gpuBefore->region == DeckRegion::Secondary);
    CHECK(!canPlace(L, WidgetId::Gpu, DeckRegion::Primary, 0, 0));
    CHECK(!moveWidget(L, WidgetId::Gpu, DeckRegion::Primary, 0, 0));
    const DeckWidgetPlacement* gpuAfter = findPlacement(L, WidgetId::Gpu);
    CHECK(gpuAfter != nullptr);
    CHECK(gpuAfter->region == DeckRegion::Secondary);
    CHECK(gpuAfter->row == gpuBefore->row && gpuAfter->column == gpuBefore->column);
}

void test_move_preserves_span_and_size() {
    DeckLayout L = defaultCommandDeckLayout();
    const DeckWidgetPlacement* before = findPlacement(L, WidgetId::Cpu);
    const InstrumentSizeMode sm = before->sizeMode;
    const int rs = before->rowSpan;
    const int cs = before->columnSpan;
    // Move CPU within its Primary region (same cell is a valid no-move).
    CHECK(moveWidget(L, WidgetId::Cpu, DeckRegion::Primary, 0, 0));
    const DeckWidgetPlacement* after = findPlacement(L, WidgetId::Cpu);
    CHECK(after->sizeMode == sm);
    CHECK(after->rowSpan == rs);
    CHECK(after->columnSpan == cs);
}

void test_disable_widget() {
    DeckLayout L = defaultCommandDeckLayout();
    CHECK(setWidgetEnabled(L, WidgetId::Storage, false));
    const DeckWidgetPlacement* s = findPlacement(L, WidgetId::Storage);
    CHECK(s != nullptr && !s->enabled);
    CHECK(isValidLayout(L));  // disabling never invalidates
}

void test_reenable_widget() {
    DeckLayout L = defaultCommandDeckLayout();
    CHECK(setWidgetEnabled(L, WidgetId::Storage, false));
    CHECK(setWidgetEnabled(L, WidgetId::Storage, true));
    const DeckWidgetPlacement* s = findPlacement(L, WidgetId::Storage);
    CHECK(s != nullptr && s->enabled);
    CHECK(isValidLayout(L));
}

void test_disable_frees_cell_for_move() {
    DeckLayout L = defaultCommandDeckLayout();
    // With Memory (0,1) disabled, GPU may move onto (0,1).
    CHECK(setWidgetEnabled(L, WidgetId::Memory, false));
    CHECK(canPlace(L, WidgetId::Gpu, DeckRegion::Secondary, 0, 1));
    CHECK(moveWidget(L, WidgetId::Gpu, DeckRegion::Secondary, 0, 1));
}

void test_absent_widget_ops_fail() {
    DeckLayout L = defaultCommandDeckLayout();
    CHECK(!canPlace(L, WidgetId::Unknown, DeckRegion::Secondary, 0, 0));
    CHECK(!moveWidget(L, WidgetId::Unknown, DeckRegion::Secondary, 0, 0));
    CHECK(!setWidgetEnabled(L, WidgetId::Unknown, false));
}

}  // namespace

int main() {
    test_find_existing_placement();
    test_move_to_valid_empty_cell();
    test_reject_overlap();
    test_reject_negative();
    test_reject_cross_region_moves();
    test_move_preserves_span_and_size();
    test_disable_widget();
    test_reenable_widget();
    test_disable_frees_cell_for_move();
    test_absent_widget_ops_fail();
    if (g_failures == 0) {
        std::puts("All DeckLayoutEdits tests passed.");
        return 0;
    }
    std::fprintf(stderr, "%d DeckLayoutEdits check(s) failed.\n", g_failures);
    return 1;
}
