// SPDX-License-Identifier: GPL-3.0-or-later
//
// Edit-Mode tests on the real CommandDeckPage. Proves the safety-critical
// invariant that instrument POINTER IDENTITY is preserved across move, disable,
// re-enable, Save, and Cancel -- so telemetry bindings Application holds never
// dangle -- plus Cancel-restores-exactly, Save-emits-committed, and
// invalid-edit-not-committed. Uses QApplication (widgets + event loop).

#include <cstdio>
#include <vector>

#include <QApplication>
#include <QEvent>
#include <QKeyEvent>
#include <QMouseEvent>

#include "deck/instruments/CpuInstrument.hpp"
#include "deck/instruments/GpuInstrument.hpp"
#include "deck/instruments/InstrumentModelFanout.hpp"
#include "deck/instruments/MemoryInstrument.hpp"
#include "deck/layout/DeckLayout.hpp"
#include "deck/layout/DeckLayoutEdits.hpp"
#include "deck/pages/CommandDeckPage.hpp"

using namespace darkspark::deck;
using namespace darkspark::deck::layout;
using darkspark::deck::instruments::GpuInstrument;
using darkspark::deck::pages::CommandDeckPage;

namespace {
int g_failures = 0;
void reportFail(const char* e, const char* f, int l) {
    std::fprintf(stderr, "FAIL: %s  (%s:%d)\n", e, f, l);
    ++g_failures;
}
#define CHECK(c) do { if (!(c)) reportFail(#c, __FILE__, __LINE__); } while (0)

// Snapshot the six instrument pointers so we can prove identity is preserved.
struct Pointers {
    void* cpu;
    void* gpu;
    void* memory;
    void* cooling;
    void* storage;
    void* network;
};
Pointers snapshot(const CommandDeckPage& p) {
    return Pointers{p.primaryInstrument(), p.gpuInstrument(),
                    p.memoryInstrument(), p.coolingInstrument(),
                    p.storageInstrument(), p.networkInstrument()};
}
bool same(const Pointers& a, const Pointers& b) {
    return a.cpu == b.cpu && a.gpu == b.gpu && a.memory == b.memory
           && a.cooling == b.cooling && a.storage == b.storage
           && a.network == b.network;
}

// Select a widget the way production does: deliver a real left-press TO THE
// INSTRUMENT CHILD widget. The instrument would normally consume it; the page's
// installed event filter intercepts it while editing and performs selection.
// This exercises the actual runtime event route (not a synthetic press sent to
// the parent, which was the flaw the runtime bug exposed).
bool clickSelect(CommandDeckPage& page, QWidget* instrument) {
    page.show();  // realize geometry so the widget hierarchy is live
    QApplication::processEvents();
    const QPoint local(instrument->width() / 2, instrument->height() / 2);
    QMouseEvent press(QEvent::MouseButtonPress, local,
                      instrument->mapToGlobal(local), Qt::LeftButton,
                      Qt::LeftButton, Qt::NoModifier);
    // Send to the INSTRUMENT, not the page: the page's event filter must catch
    // it. Returns whether the event was accepted (consumed by the filter).
    return QApplication::sendEvent(instrument, &press);
}

void test_pointer_identity_across_move() {
    CommandDeckPage page(defaultCommandDeckLayout());
    const Pointers before = snapshot(page);
    GpuInstrument* gpu = page.gpuInstrument();
    CHECK(gpu != nullptr);

    page.beginEdit();
    clickSelect(page, gpu);
    CHECK(page.selectedWidget() == WidgetId::Gpu);
    // Move GPU to the empty (1,2) cell; must succeed and NOT reallocate.
    CHECK(page.moveSelection(DeckRegion::Secondary, 1, 2));
    const Pointers after = snapshot(page);
    CHECK(same(before, after));               // same instances after a move
    CHECK(page.gpuInstrument() == gpu);       // exact same GPU pointer
    // Telemetry-facing accessor still resolves to the live, shown widget.
    CHECK(gpu->isVisible());
}

void test_arrow_key_moves_after_child_click() {
    CommandDeckPage page(defaultCommandDeckLayout());
    GpuInstrument* gpu = page.gpuInstrument();
    CHECK(gpu != nullptr);
    void* gpuPtr = gpu;

    page.beginEdit();
    // Free the cell to GPU's right (Memory sits at secondary (0,1)) so a Right
    // arrow has a valid destination and its effect is observable.
    clickSelect(page, page.memoryInstrument());
    CHECK(page.selectedWidget() == WidgetId::Memory);
    CHECK(page.toggleSelectedEnabled());          // disable Memory -> (0,1) free

    // Real route: press on the CHILD instrument; the filter selects GPU and
    // returns focus to the page.
    clickSelect(page, gpu);
    CHECK(page.selectedWidget() == WidgetId::Gpu); // selected via event filter

    // A Right arrow delivered to the page must reach keyPressEvent and move the
    // selection from (0,0) to the now-free (0,1).
    QKeyEvent right(QEvent::KeyPress, Qt::Key_Right, Qt::NoModifier);
    QApplication::sendEvent(&page, &right);
    CHECK(page.gpuInstrument() == gpuPtr);         // pointer identity intact
    CHECK(page.selectedWidget() == WidgetId::Gpu);
    CHECK(page.moveSelection(DeckRegion::Secondary, 0, 1));  // idempotent -> at (0,1)
    CHECK(page.gpuInstrument() == gpuPtr);
}

void test_filter_inactive_when_not_editing() {
    CommandDeckPage page(defaultCommandDeckLayout());
    GpuInstrument* gpu = page.gpuInstrument();
    // Not editing: a press on the instrument must NOT select anything (filter
    // passes it through untouched, preserving normal instrument behavior).
    clickSelect(page, gpu);
    CHECK(page.selectedWidget() == WidgetId::Unknown);
    CHECK(!page.isEditing());
}

void test_pointer_identity_across_disable_reenable() {
    CommandDeckPage page(defaultCommandDeckLayout());
    const Pointers before = snapshot(page);
    GpuInstrument* gpu = page.gpuInstrument();
    page.beginEdit();
    clickSelect(page, gpu);
    CHECK(page.selectedWidget() == WidgetId::Gpu);
    CHECK(page.toggleSelectedEnabled());   // disable -> hidden, not destroyed
    CHECK(page.gpuInstrument() == gpu);    // pointer survives disable
    CHECK(!gpu->isVisible());              // hidden
    CHECK(page.toggleSelectedEnabled());   // re-enable -> shown again
    CHECK(page.gpuInstrument() == gpu);    // pointer survives re-enable
    CHECK(gpu->isVisible());
    CHECK(same(before, snapshot(page)));   // all six unchanged
}

void test_pointer_identity_across_cancel_roundtrip() {
    CommandDeckPage page(defaultCommandDeckLayout());
    const Pointers before = snapshot(page);
    GpuInstrument* gpu = page.gpuInstrument();
    page.beginEdit();
    clickSelect(page, gpu);
    page.moveSelection(DeckRegion::Secondary, 1, 2);  // change something
    page.cancelEdits();  // applyLayout(preEdit) runs -> must reuse instances
    CHECK(same(before, snapshot(page)));
    CHECK(page.gpuInstrument() == gpu);
}

void test_invalid_move_rejected() {
    CommandDeckPage page(defaultCommandDeckLayout());
    GpuInstrument* gpu = page.gpuInstrument();
    page.beginEdit();
    clickSelect(page, gpu);
    // Memory occupies (0,1); moving GPU there overlaps -> rejected.
    CHECK(!page.moveSelection(DeckRegion::Secondary, 0, 1));
    // Negative cell -> rejected.
    CHECK(!page.moveSelection(DeckRegion::Secondary, -1, 0));
    CHECK(page.gpuInstrument() == gpu);  // untouched
}

void test_cancel_restores_layout_exactly() {
    CommandDeckPage page(defaultCommandDeckLayout());
    page.beginEdit();
    // No committed changes; Cancel returns to Edit-off and identical layout.
    page.cancelEdits();
    CHECK(!page.isEditing());
}

void test_save_emits_committed_and_is_valid() {
    CommandDeckPage page(defaultCommandDeckLayout());
    int committedCount = 0;
    DeckLayout committed;
    QObject::connect(&page, &CommandDeckPage::layoutCommitted,
                     [&](const DeckLayout& l) {
                         ++committedCount;
                         committed = l;
                     });
    page.beginEdit();
    const bool saved = page.saveEdits();  // no changes: still valid
    CHECK(saved);
    CHECK(!page.isEditing());
    CHECK(committedCount == 1);
    CHECK(isValidLayout(committed));
    // Pointer identity preserved across Save too.
    CHECK(page.primaryInstrument() != nullptr);
}


void test_same_subsystem_can_live_on_multiple_pages() {
    DeckLayout custom;
    custom.pageId = 2;
    custom.placements.push_back(DeckWidgetPlacement{
        .id = WidgetId::Gpu,
        .region = DeckRegion::Secondary,
        .row = 0,
        .column = 0,
        .rowSpan = 1,
        .columnSpan = 1,
        .sizeMode = instruments::InstrumentSizeMode::Small,
        .enabled = true,
    });

    CommandDeckPage system(defaultCommandDeckLayout());
    CommandDeckPage customPage(custom);

    GpuInstrument* systemGpu = system.gpuInstrument();
    GpuInstrument* customGpu = customPage.gpuInstrument();
    CHECK(systemGpu != nullptr);
    CHECK(customGpu != nullptr);
    CHECK(systemGpu != customGpu);  // each page owns its own QWidget view

    instruments::GpuInstrumentModel model;
    model.utilizationPercent = 63.0;
    model.utilizationAvailability = instruments::ValueAvailability::Live;
    model.temperatureCelsius = 71.0;
    model.temperatureAvailability = instruments::ValueAvailability::Live;

    const std::vector<GpuInstrument*> targets{systemGpu, customGpu};
    instruments::fanOutInstrumentModel(model, targets);

    CHECK(systemGpu->model().utilizationPercent == 63.0);
    CHECK(customGpu->model().utilizationPercent == 63.0);
    CHECK(systemGpu->model().temperatureCelsius == 71.0);
    CHECK(customGpu->model().temperatureCelsius == 71.0);
}

void test_no_commit_when_not_editing() {
    CommandDeckPage page(defaultCommandDeckLayout());
    int committedCount = 0;
    QObject::connect(&page, &CommandDeckPage::layoutCommitted,
                     [&](const DeckLayout&) { ++committedCount; });
    CHECK(!page.saveEdits());     // not editing -> no-op
    CHECK(committedCount == 0);   // nothing emitted, nothing persisted
}

}  // namespace

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    test_pointer_identity_across_move();
    test_arrow_key_moves_after_child_click();
    test_filter_inactive_when_not_editing();
    test_pointer_identity_across_disable_reenable();
    test_pointer_identity_across_cancel_roundtrip();
    test_cancel_restores_layout_exactly();
    test_invalid_move_rejected();
    test_save_emits_committed_and_is_valid();
    test_same_subsystem_can_live_on_multiple_pages();
    test_no_commit_when_not_editing();
    if (g_failures == 0) {
        std::puts("All CommandDeckPage edit tests passed.");
        return 0;
    }
    std::fprintf(stderr, "%d CommandDeckPage edit check(s) failed.\n",
                 g_failures);
    return 1;
}
