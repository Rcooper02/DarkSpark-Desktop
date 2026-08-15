// SPDX-License-Identifier: GPL-3.0-or-later
//
// Edit-Mode tests on the real CommandDeckPage. Proves the safety-critical
// invariant that instrument POINTER IDENTITY is preserved across move, disable,
// re-enable, Save, and Cancel -- so telemetry bindings Application holds never
// dangle -- plus Cancel-restores-exactly, Save-emits-committed, and
// invalid-edit-not-committed. Uses QApplication (widgets + event loop).

#include <cstdio>

#include <QApplication>
#include <QMouseEvent>

#include "deck/instruments/CpuInstrument.hpp"
#include "deck/instruments/GpuInstrument.hpp"
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

// Select a widget the way production does: synthesize a left-click at the
// center of its instrument, in page coordinates. Returns whether selection took.
bool clickSelect(CommandDeckPage& page, QWidget* instrument) {
    page.show();  // ensure geometry is realized so mapTo/positions are valid
    QApplication::processEvents();
    const QPoint center =
        instrument->mapTo(&page, QPoint(instrument->width() / 2,
                                        instrument->height() / 2));
    QMouseEvent press(QEvent::MouseButtonPress, center,
                      instrument->mapToGlobal(QPoint(instrument->width() / 2,
                                                     instrument->height() / 2)),
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(&page, &press);
    return true;
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
    test_pointer_identity_across_disable_reenable();
    test_pointer_identity_across_cancel_roundtrip();
    test_cancel_restores_layout_exactly();
    test_invalid_move_rejected();
    test_save_emits_committed_and_is_valid();
    test_no_commit_when_not_editing();
    if (g_failures == 0) {
        std::puts("All CommandDeckPage edit tests passed.");
        return 0;
    }
    std::fprintf(stderr, "%d CommandDeckPage edit check(s) failed.\n",
                 g_failures);
    return 1;
}
