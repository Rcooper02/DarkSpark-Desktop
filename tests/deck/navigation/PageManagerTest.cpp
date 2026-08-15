// SPDX-License-Identifier: GPL-3.0-or-later
//
// Tests for PageManager as a page-type-agnostic host: it accepts plain QWidgets
// (the Batch-3 addPage(QWidget*) relaxation), reports page count and active
// index, navigates among three pages, and emits activePageChanged. Uses
// QApplication (widgets + event loop); own main().

#include <cstdio>

#include <QApplication>
#include <QEventLoop>
#include <QTimer>
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

template <typename Action>
bool navigateAndWait(PageManager& mgr, int expectedIndex, Action action) {
    bool settled = false;
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);

    QObject::connect(&mgr, &PageManager::activePageChanged, &loop,
                     [&loop, &settled, expectedIndex](int index) {
                         if (index == expectedIndex) {
                             settled = true;
                             loop.quit();
                         }
                     });

    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);

    action();

    // PageTransition may settle synchronously in its instant-fallback path.
    if (settled || mgr.activeIndex() == expectedIndex) {
        return mgr.activeIndex() == expectedIndex;
    }

    timeout.start(2000);
    loop.exec();

    return settled && mgr.activeIndex() == expectedIndex;
}

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

    CHECK(navigateAndWait(mgr, 2, [&mgr]() { mgr.goToPage(2); }));
    CHECK(navigateAndWait(mgr, 1, [&mgr]() { mgr.goToPage(1); }));
    CHECK(navigateAndWait(mgr, 0, [&mgr]() { mgr.previousPage(); }));
    CHECK(navigateAndWait(mgr, 1, [&mgr]() { mgr.nextPage(); }));

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
    CHECK(navigateAndWait(mgr, 2, [&mgr]() { mgr.goToPage(2); }));
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
