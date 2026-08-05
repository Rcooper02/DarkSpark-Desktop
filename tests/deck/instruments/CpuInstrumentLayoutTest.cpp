// SPDX-License-Identifier: GPL-3.0-or-later
//
// Pure geometry tests for the CPU instrument layout. Qt-free and
// display-free: the layout is separated from painting precisely so its
// composition can be verified deterministically here.

#include <cmath>
#include <cstdio>

#include "deck/instruments/CpuInstrumentLayout.hpp"

using namespace darkspark::deck::instruments;
using namespace darkspark::deck::instruments::layout_constants;

namespace {
int g_failures = 0;
void reportFail(const char* expr, const char* file, int line) {
    std::fprintf(stderr, "FAIL: %s  (%s:%d)\n", expr, file, line);
    ++g_failures;
}
#define CHECK(cond) \
    do { if (!(cond)) reportFail(#cond, __FILE__, __LINE__); } while (0)

bool near(double a, double b) { return std::fabs(a - b) < 1e-9; }

void test_positional_fraction() {
    CHECK(near(positionalFraction(42.0, 0.0, 100.0), 0.42));
    CHECK(near(positionalFraction(61.0, 20.0, 100.0), 0.5125));  // temp mapping
    CHECK(near(positionalFraction(-5.0, 0.0, 100.0), 0.0));      // clamp low
    CHECK(near(positionalFraction(150.0, 0.0, 100.0), 1.0));     // clamp high
    CHECK(near(positionalFraction(10.0, 20.0, 100.0), 0.0));     // below span
    CHECK(near(positionalFraction(5.0, 5.0, 5.0), 0.0));         // degenerate
}

void test_large_composition() {
    // The 4th argument is now a pre-computed secondary fraction [0, 1] (the CPU
    // instrument maps temperature onto it via positionalFraction, tested
    // separately above). 0.5125 is what a 61 C reading maps to over [20, 100].
    const auto L =
        resolveCpuInstrumentLayout(InstrumentSizeMode::Large, 400.0, 0.42, 0.5125);
    CHECK(L.hasInnerRing);
    CHECK(L.temperatureRing.present);
    CHECK(L.utilizationRing.segments == 38);
    CHECK(L.temperatureRing.segments == 38);
    // Concentric hierarchy: outer bigger and thicker than inner.
    CHECK(L.utilizationRing.outerRadius > L.temperatureRing.outerRadius);
    CHECK(L.utilizationRing.thickness > L.temperatureRing.thickness);
    // Outer fill is positional from utilization; inner fill is the supplied
    // secondary fraction passed straight through.
    CHECK(near(L.utilizationRing.fillFraction, 0.42));
    CHECK(near(L.temperatureRing.fillFraction, 0.5125));
    // Responsive primary: min(64, 0.155 * 400 = 62) = 62.
    CHECK(near(L.primaryValuePx, 62.0));
    CHECK(L.hasTrendBand);
    CHECK(near(L.arcSpanDegrees, 270.0));
    CHECK(near(L.arcStartDegrees, 135.0));
    // Value sits at (near) the optical center -- nudged a hair above geometric
    // center so the larger primary balances against the reserved trend band.
    CHECK(std::fabs(L.valueY - L.centerY) < L.side * 0.02);
}

void test_large_responsive_primary_scales_down() {
    // At a smaller square the primary scales with the box: min(64,0.155*200=31).
    const auto L =
        resolveCpuInstrumentLayout(InstrumentSizeMode::Large, 200.0, 0.42, 0.5125);
    CHECK(near(L.primaryValuePx, 31.0));
}

void test_small_composition_is_reduced_not_shrunk() {
    const auto S =
        resolveCpuInstrumentLayout(InstrumentSizeMode::Small, 400.0, 0.42, 0.5125);
    // Deliberately reduced: single arc, no inner ring, no trend band.
    CHECK(!S.hasInnerRing);
    CHECK(!S.temperatureRing.present);
    CHECK(!S.hasTrendBand);
    // Fewer, wider segments keep the segmented identity legible when small.
    CHECK(S.utilizationRing.segments == 18);
    // Family silhouette preserved: same gapped 270-degree sweep.
    CHECK(S.hasStatusGap);
    CHECK(near(S.arcSpanDegrees, 270.0));
    CHECK(near(S.utilizationRing.fillFraction, 0.42));
}

void test_medium_wide_fall_back_to_large() {
    const auto M =
        resolveCpuInstrumentLayout(InstrumentSizeMode::Medium, 400.0, 0.42, 0.5125);
    const auto W =
        resolveCpuInstrumentLayout(InstrumentSizeMode::Wide, 400.0, 0.42, 0.5125);
    CHECK(M.hasInnerRing);  // until dedicated compositions exist
    CHECK(W.hasInnerRing);
}

void test_fill_fraction_edges() {
    const auto zero =
        resolveCpuInstrumentLayout(InstrumentSizeMode::Large, 400.0, 0.0, 0.0);
    CHECK(near(zero.utilizationRing.fillFraction, 0.0));
    CHECK(near(zero.temperatureRing.fillFraction, 0.0));
    const auto full = resolveCpuInstrumentLayout(InstrumentSizeMode::Large,
                                                 400.0, 1.0, 1.0);
    CHECK(near(full.utilizationRing.fillFraction, 1.0));
    CHECK(near(full.temperatureRing.fillFraction, 1.0));
}

}  // namespace

int main() {
    test_positional_fraction();
    test_large_composition();
    test_large_responsive_primary_scales_down();
    test_small_composition_is_reduced_not_shrunk();
    test_medium_wide_fall_back_to_large();
    test_fill_fraction_edges();

    if (g_failures == 0) {
        std::puts("All CpuInstrumentLayout tests passed.");
        return 0;
    }
    std::fprintf(stderr, "%d check(s) failed.\n", g_failures);
    return 1;
}
