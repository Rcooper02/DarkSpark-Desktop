// SPDX-License-Identifier: GPL-3.0-or-later
#include <cstdio>

#include "deck/controls/CompanionControl.hpp"

using darkspark::deck::controls::CompanionControl;

namespace {
int g_failures = 0;
void reportFail(const char* e, const char* f, int l) {
    std::fprintf(stderr, "FAIL: %s (%s:%d)\n", e, f, l);
    ++g_failures;
}
#define CHECK(c) do { if (!(c)) reportFail(#c, __FILE__, __LINE__); } while (0)

void test_validation() {
    const CompanionControl good{QStringLiteral("TEST"), QString{}, 1, 0, 3};
    CHECK(good.isValid());

    CompanionControl bad = good;
    bad.label.clear();
    CHECK(!bad.isValid());

    bad = good;
    bad.row = -1;
    CHECK(!bad.isValid());
}
}  // namespace

int main() {
    test_validation();
    if (g_failures == 0) {
        std::puts("All CompanionControl tests passed.");
        return 0;
    }
    std::fprintf(stderr, "%d CompanionControl check(s) failed.\n", g_failures);
    return 1;
}
