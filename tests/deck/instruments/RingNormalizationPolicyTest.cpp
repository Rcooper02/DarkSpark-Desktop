// SPDX-License-Identifier: GPL-3.0-or-later
//
// Deterministic tests for RingNormalizationPolicy implementations. Qt-free.
// The policy produces visualization state only: a [0,1] fill, never a
// percentage or capacity claim.

#include <cmath>
#include <cstdio>

#include "deck/instruments/RingNormalizationPolicy.hpp"

using namespace darkspark::deck::instruments;

namespace {
int g_failures = 0;
void reportFail(const char* e, const char* f, int l) {
    std::fprintf(stderr, "FAIL: %s  (%s:%d)\n", e, f, l);
    ++g_failures;
}
#define CHECK(c) do { if (!(c)) reportFail(#c, __FILE__, __LINE__); } while (0)

void test_always_in_unit_range() {
    AdaptiveObservedMaxPolicy p;
    for (double v = 0.0; v < 12000.0; v += 91.0) {
        const double f = p.normalize(v);
        CHECK(f >= 0.0 && f <= 1.0);
    }
}

void test_zero_is_empty() {
    AdaptiveObservedMaxPolicy p;
    CHECK(p.normalize(0.0) == 0.0);
}

void test_new_peak_fills_near_full() {
    AdaptiveObservedMaxPolicy p(800.0, 0.02);
    // A value at a fresh peak should read near-full (value == observedMax).
    const double f = p.normalize(2000.0);
    CHECK(f > 0.95);
}

void test_floor_prevents_pegging_small_values() {
    AdaptiveObservedMaxPolicy p(1000.0, 0.02);
    // A small first value, well under the floor, must not peg the ring.
    const double f = p.normalize(200.0);
    CHECK(f < 0.5);
}

void test_spike_relaxes_over_time() {
    AdaptiveObservedMaxPolicy p(800.0, 0.05);
    (void)p.normalize(3000.0);        // spike sets a high observed max
    const double afterSpike = p.observedMaxForTest();
    for (int i = 0; i < 50; ++i) {
        (void)p.normalize(1000.0);   // sustained lower value
    }
    // observedMax should have decayed toward the sustained value.
    CHECK(p.observedMaxForTest() < afterSpike);
}

void test_never_emits_percentage_semantics() {
    // The policy returns a fraction; a caller multiplying by 100 would get a
    // number, but the policy itself never claims capacity. This is a
    // documentation guarantee; here we just assert monotonic behaviour:
    // a larger value (at or below the current max) never fills less than a
    // smaller one within the same observed window.
    AdaptiveObservedMaxPolicy p(800.0, 0.0);  // no decay: fixed observed max
    (void)p.normalize(2000.0);                  // observedMax = 2000
    const double small = p.normalize(500.0);
    const double large = p.normalize(1500.0);
    CHECK(large > small);
}

}  // namespace

int main() {
    test_always_in_unit_range();
    test_zero_is_empty();
    test_new_peak_fills_near_full();
    test_floor_prevents_pegging_small_values();
    test_spike_relaxes_over_time();
    test_never_emits_percentage_semantics();
    if (g_failures == 0) {
        std::puts("All RingNormalizationPolicy tests passed.");
        return 0;
    }
    std::fprintf(stderr, "%d normalization check(s) failed.\n", g_failures);
    return 1;
}
