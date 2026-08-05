// SPDX-License-Identifier: GPL-3.0-or-later
//
// Deterministic tests for ThroughputRateCalculator. Qt-free.
// Covers: first observation Unavailable, correct second-observation rate,
// counter reset/decrease Unavailable, elapsed-time validation.

#include <cstdio>

#include "services/ThroughputRateCalculator.hpp"

using namespace darkspark::services;

namespace {
int g_failures = 0;
void reportFail(const char* e, const char* f, int l) {
    std::fprintf(stderr, "FAIL: %s  (%s:%d)\n", e, f, l);
    ++g_failures;
}
#define CHECK(c) do { if (!(c)) reportFail(#c, __FILE__, __LINE__); } while (0)

void test_first_observation_unavailable() {
    ThroughputRateCalculator c;
    CHECK(!c.update(1000, 0).has_value());  // no prior sample -> Unavailable
}

void test_second_observation_rate() {
    ThroughputRateCalculator c;
    (void)c.update(1000, 0);
    const auto r = c.update(3000, 1000);  // +2000 bytes / 1s
    CHECK(r.has_value());
    if (r) CHECK(*r == 2000.0);
}

void test_sustained_rates() {
    ThroughputRateCalculator c;
    (void)c.update(0, 0);
    auto r1 = c.update(1000, 1000);  // 1000 B/s
    auto r2 = c.update(3000, 2000);  // 2000 B/s
    CHECK(r1 && *r1 == 1000.0);
    CHECK(r2 && *r2 == 2000.0);
}

void test_counter_reset_unavailable() {
    ThroughputRateCalculator c;
    (void)c.update(5000, 0);
    (void)c.update(9000, 1000);
    const auto r = c.update(500, 2000);  // counter decreased -> reset
    CHECK(!r.has_value());
    // After reset, the next observation is treated as first-after-prime.
    const auto r2 = c.update(1500, 3000);  // +1000 over 1s from re-primed 500
    CHECK(r2.has_value());
    if (r2) CHECK(*r2 == 1000.0);
}

void test_nonpositive_interval_unavailable() {
    ThroughputRateCalculator c;
    (void)c.update(1000, 5000);
    const auto same = c.update(2000, 5000);  // clock did not advance
    CHECK(!same.has_value());
    const auto back = c.update(3000, 4000);  // clock went backwards
    CHECK(!back.has_value());
}

void test_reset_method() {
    ThroughputRateCalculator c;
    (void)c.update(1000, 0);
    (void)c.update(2000, 1000);
    c.reset();
    CHECK(!c.update(9999, 2000).has_value());  // first-after-reset -> Unavailable
}

void test_never_negative() {
    ThroughputRateCalculator c;
    (void)c.update(1000, 0);
    // Any decrease returns nullopt, never a negative rate.
    for (std::uint64_t v : {900ULL, 800ULL, 0ULL}) {
        const auto r = c.update(v, 1000);
        CHECK(!r.has_value() || *r >= 0.0);
    }
}

}  // namespace

int main() {
    test_first_observation_unavailable();
    test_second_observation_rate();
    test_sustained_rates();
    test_counter_reset_unavailable();
    test_nonpositive_interval_unavailable();
    test_reset_method();
    test_never_negative();
    if (g_failures == 0) {
        std::puts("All ThroughputRateCalculator tests passed.");
        return 0;
    }
    std::fprintf(stderr, "%d rate-calc check(s) failed.\n", g_failures);
    return 1;
}
