// SPDX-License-Identifier: GPL-3.0-or-later
//
// Tests for PageManager as a page-type-agnostic host: it accepts plain QWidgets
// (the Batch-3 addPage(QWidget*) relaxation), reports page count and active
// index, navigates among three pages, and emits activePageChanged. Uses
// QApplication (widgets + event loop); own main().

#include <cstdio>

#include <QApplication>
#include <QWidget>

#include "deck/navigation/PageManager.hpp"

using namespace darkspark::deck::navigation;

namespace {
int g_failures = 0;
void reportFail(const char* e, const char* f, int l) {
    std::fprintf(stderr, "FAIL: %s  (%s:%d)\n", e, f, l);
    ++g_failures;
}
#define CHECK(c) do { if (!(c)) reportFail(#c, __FILE__, __LINE__); } while (0)

void test_add_plain_qwidgets() {
    PageManager mgr;
    CHECK(mgr.pageCount() == 0);
    mgr.addPage(new QWidget());  // plain QWidget, not a DeckPage
    mgr.addPage(new QWidget());
    mgr.addPage(new QWidget());
    CHECK(mgr.pageCount() == 3);
    CHECK(mgr.activeIndex() == 0);  // first page added becomes active
}

void test_null_page_ignored() {
    PageManager mgr;
    mgr.addPage(nullptr);
    CHECK(mgr.pageCount() == 0);
}

void test_navigation_among_three_pages() {
    PageManager mgr;
    mgr.addPage(new QWidget());
    mgr.addPage(new QWidget());
    mgr.addPage(new QWidget());

    mgr.goToPage(2);
    CHECK(mgr.activeIndex() == 2);
    mgr.goToPage(1);
    CHECK(mgr.activeIndex() == 1);
    mgr.previousPage();
    CHECK(mgr.activeIndex() == 0);
    mgr.nextPage();
    CHECK(mgr.activeIndex() == 1);

    // Out-of-range requests are ignored (no change, no crash).
    mgr.goToPage(99);
    CHECK(mgr.activeIndex() == 1);
    mgr.goToPage(-1);
    CHECK(mgr.activeIndex() == 1);
}

void test_active_page_changed_signal() {
    PageManager mgr;
    mgr.addPage(new QWidget());
    mgr.addPage(new QWidget());
    mgr.addPage(new QWidget());
    int lastIndex = -1;
    int count = 0;
    QObject::connect(&mgr, &PageManager::activePageChanged,
                     [&lastIndex, &count](int index) {
                         lastIndex = index;
                         ++count;
                     });
    mgr.goToPage(2);
    // The transition may be animated; process events so it completes.
    QCoreApplication::processEvents();
    CHECK(count >= 1);
    CHECK(lastIndex == 2);
}

}  // namespace

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    test_add_plain_qwidgets();
    test_null_page_ignored();
    test_navigation_among_three_pages();
    test_active_page_changed_signal();
    if (g_failures == 0) {
        std::puts("All PageManager tests passed.");
        return 0;
    }
    std::fprintf(stderr, "%d PageManager check(s) failed.\n", g_failures);
    return 1;
}
