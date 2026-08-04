// SPDX-License-Identifier: GPL-3.0-or-later
//
// Deterministic tests for the Memory adapter: folding telemetry samples into
// the Memory presentation model, joining the utilization percentage with the
// used/total byte figures and deriving available. Qt-free and display-free.

#include <cmath>
#include <cstdio>

#include "deck/instruments/MemoryInstrumentModelAdapter.hpp"
#include "models/MetricSample.hpp"

using namespace darkspark::deck::instruments;
using namespace darkspark::models;

namespace {
int g_failures = 0;
void reportFail(const char* e, const char* f, int l) {
    std::fprintf(stderr, "FAIL: %s  (%s:%d)\n", e, f, l);
    ++g_failures;
}
#define CHECK(c) do { if (!(c)) reportFail(#c, __FILE__, __LINE__); } while (0)
bool near(double a, double b) { return std::fabs(a - b) < 1e-6; }

constexpr double kGiB = 1024.0 * 1024.0 * 1024.0;

MetricSample pct(double v, MetricState st = MetricState::Fresh) {
    if (st == MetricState::Unavailable)
        return MetricSample::unavailable(MetricId::MemoryUtilization, 1);
    if (st == MetricState::Stale)
        return *MetricSample::tryStale(MetricId::MemoryUtilization, v,
                                       MetricUnit::Percent, 1);
    return *MetricSample::tryFresh(MetricId::MemoryUtilization, v,
                                   MetricUnit::Percent, 1);
}
MetricSample used(double v, MetricState st = MetricState::Fresh) {
    if (st == MetricState::Unavailable)
        return MetricSample::unavailable(MetricId::MemoryUsedBytes, 1);
    if (st == MetricState::Stale)
        return *MetricSample::tryStale(MetricId::MemoryUsedBytes, v,
                                       MetricUnit::Bytes, 1);
    return *MetricSample::tryFresh(MetricId::MemoryUsedBytes, v,
                                   MetricUnit::Bytes, 1);
}
MetricSample total(double v, MetricState st = MetricState::Fresh) {
    if (st == MetricState::Unavailable)
        return MetricSample::unavailable(MetricId::MemoryTotalBytes, 1);
    if (st == MetricState::Stale)
        return *MetricSample::tryStale(MetricId::MemoryTotalBytes, v,
                                       MetricUnit::Bytes, 1);
    return *MetricSample::tryFresh(MetricId::MemoryTotalBytes, v,
                                   MetricUnit::Bytes, 1);
}

// --- Percentage calculation / passthrough ----------------------------------
void test_percentage() {
    MemoryInstrumentModelAdapter a;
    CHECK(a.apply(pct(37.5)));
    CHECK(near(a.model().utilizationPercent, 37.5));
    CHECK(a.model().utilizationAvailability == ValueAvailability::Live);
}

// --- Used/total byte join + derived available ------------------------------
void test_used_total_and_derived_available() {
    MemoryInstrumentModelAdapter a;
    CHECK(a.apply(used(8.0 * kGiB)));
    // Available not derivable yet: total still Absent.
    CHECK(a.model().availableAvailability == ValueAvailability::Absent);
    CHECK(a.apply(total(32.0 * kGiB)));
    CHECK(near(a.model().usedBytes, 8.0 * kGiB));
    CHECK(near(a.model().totalBytes, 32.0 * kGiB));
    // Derived available = total - used.
    CHECK(near(a.model().availableBytes, 24.0 * kGiB));
    CHECK(a.model().availableAvailability == ValueAvailability::Live);
}

// --- Fresh / Stale / Unavailable mapping -----------------------------------
void test_state_mapping() {
    MemoryInstrumentModelAdapter a;
    a.apply(pct(50.0, MetricState::Fresh));
    CHECK(a.model().utilizationAvailability == ValueAvailability::Live);
    a.apply(pct(50.0, MetricState::Stale));
    CHECK(a.model().utilizationAvailability == ValueAvailability::LastKnown);
    a.apply(pct(0.0, MetricState::Unavailable));
    CHECK(a.model().utilizationAvailability == ValueAvailability::Absent);
}

// Stale used/total => derived available is LastKnown (least-fresh input wins).
void test_derived_available_staleness() {
    MemoryInstrumentModelAdapter a;
    a.apply(used(4.0 * kGiB, MetricState::Fresh));
    a.apply(total(16.0 * kGiB, MetricState::Stale));
    CHECK(a.model().availableAvailability == ValueAvailability::LastKnown);
    CHECK(near(a.model().availableBytes, 12.0 * kGiB));
}

// Unavailable used => available not derivable.
void test_available_absent_when_input_absent() {
    MemoryInstrumentModelAdapter a;
    a.apply(used(4.0 * kGiB));
    a.apply(total(16.0 * kGiB));
    CHECK(a.model().availableAvailability == ValueAvailability::Live);
    a.apply(used(0.0, MetricState::Unavailable));
    CHECK(a.model().availableAvailability == ValueAvailability::Absent);
}

// --- Malformed / impossible values -----------------------------------------
void test_negative_bytes_clamped() {
    MemoryInstrumentModelAdapter a;
    a.apply(used(-5.0));  // impossible negative
    CHECK(a.model().usedBytes >= 0.0);
}
void test_used_exceeds_total_available_floored() {
    MemoryInstrumentModelAdapter a;
    // used > total (transient inconsistency between two samples): available
    // must floor at zero, never go negative.
    a.apply(used(20.0 * kGiB));
    a.apply(total(16.0 * kGiB));
    CHECK(a.model().availableBytes >= 0.0);
    CHECK(near(a.model().availableBytes, 0.0));
}

// --- Clamping (percentage) --------------------------------------------------
void test_percentage_clamped() {
    MemoryInstrumentModelAdapter a;
    a.apply(pct(250.0));
    CHECK(near(a.model().utilizationPercent, 100.0));
    a.apply(pct(-40.0));
    CHECK(near(a.model().utilizationPercent, 0.0));
}

// --- Adapter ignores unrelated metrics -------------------------------------
void test_ignores_unrelated() {
    MemoryInstrumentModelAdapter a;
    CHECK(!a.apply(*MetricSample::tryFresh(MetricId::CpuTotalUtilization, 50.0,
                                           MetricUnit::Percent, 1)));
    CHECK(!a.apply(*MetricSample::tryFresh(MetricId::CpuTemperature, 60.0,
                                           MetricUnit::Celsius, 1)));
    CHECK(!a.apply(*MetricSample::tryFresh(MetricId::GpuTotalUtilization, 70.0,
                                           MetricUnit::Percent, 1)));
    CHECK(!a.apply(*MetricSample::tryFresh(MetricId::GpuTemperature, 65.0,
                                           MetricUnit::Celsius, 1)));
    // None of the ignored samples disturbed the (still-initial) model.
    CHECK(a.model().utilizationAvailability == ValueAvailability::Absent);
    CHECK(a.model().usedAvailability == ValueAvailability::Absent);
    CHECK(a.model().totalAvailability == ValueAvailability::Absent);
}

// --- Initial state is all-Absent -------------------------------------------
void test_initial_absent() {
    MemoryInstrumentModelAdapter a;
    CHECK(a.model().utilizationAvailability == ValueAvailability::Absent);
    CHECK(a.model().usedAvailability == ValueAvailability::Absent);
    CHECK(a.model().totalAvailability == ValueAvailability::Absent);
    CHECK(a.model().availableAvailability == ValueAvailability::Absent);
}

// --- reset() restores initial state ----------------------------------------
void test_reset() {
    MemoryInstrumentModelAdapter a;
    a.apply(pct(50.0));
    a.apply(used(8.0 * kGiB));
    a.apply(total(32.0 * kGiB));
    a.reset();
    CHECK(a.model().utilizationAvailability == ValueAvailability::Absent);
    CHECK(a.model().availableAvailability == ValueAvailability::Absent);
}

// --- Independence: three samples in any order converge ----------------------
void test_order_independence() {
    MemoryInstrumentModelAdapter a;
    a.apply(total(32.0 * kGiB));
    a.apply(pct(25.0));
    a.apply(used(8.0 * kGiB));
    CHECK(near(a.model().utilizationPercent, 25.0));
    CHECK(near(a.model().availableBytes, 24.0 * kGiB));
    CHECK(a.model().availableAvailability == ValueAvailability::Live);
}

}  // namespace

int main() {
    test_percentage();
    test_used_total_and_derived_available();
    test_state_mapping();
    test_derived_available_staleness();
    test_available_absent_when_input_absent();
    test_negative_bytes_clamped();
    test_used_exceeds_total_available_floored();
    test_percentage_clamped();
    test_ignores_unrelated();
    test_initial_absent();
    test_reset();
    test_order_independence();
    if (g_failures == 0) {
        std::puts("All MemoryInstrumentModelAdapter tests passed.");
        return 0;
    }
    std::fprintf(stderr, "%d memory adapter check(s) failed.\n", g_failures);
    return 1;
}
