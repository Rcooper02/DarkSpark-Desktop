// SPDX-License-Identifier: GPL-3.0-or-later
//
// Deterministic tests for the role-based Cooling adapter. Qt-free.

#include <cmath>
#include <cstdio>

#include "deck/instruments/CoolingInstrumentModelAdapter.hpp"
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

MetricSample primary(double v, MetricState st = MetricState::Fresh,
                     const char* key = "prov:primary") {
    if (st == MetricState::Unavailable)
        return MetricSample::unavailable(MetricId::CoolingPrimary, 1, key);
    if (st == MetricState::Stale)
        return *MetricSample::tryStale(MetricId::CoolingPrimary, v,
                                       MetricUnit::Rpm, 1, key);
    return *MetricSample::tryFresh(MetricId::CoolingPrimary, v, MetricUnit::Rpm,
                                   1, key);
}

void test_primary_rpm() {
    CoolingInstrumentModelAdapter a;
    CHECK(a.apply(primary(1850.0)));
    CHECK(near(a.model().primaryRpm, 1850.0));
    CHECK(a.model().primaryAvailability == ValueAvailability::Live);
}

void test_coolant_secondary() {
    CoolingInstrumentModelAdapter a;
    CHECK(a.apply(*MetricSample::tryFresh(MetricId::CoolingCoolantTemp, 34.5,
                                          MetricUnit::Celsius, 1, "prov:temp")));
    CHECK(near(a.model().secondaryTempCelsius, 34.5));
    CHECK(a.model().secondaryTempAvailability == ValueAvailability::Live);
}

void test_second_fan_secondary() {
    CoolingInstrumentModelAdapter a;
    CHECK(a.apply(*MetricSample::tryFresh(MetricId::CoolingSecondary, 900.0,
                                          MetricUnit::Rpm, 1, "prov:fan2")));
    CHECK(near(a.model().secondaryRpm, 900.0));
    CHECK(a.model().secondaryRpmAvailability == ValueAvailability::Live);
}

void test_state_mapping() {
    CoolingInstrumentModelAdapter a;
    a.apply(primary(1800.0, MetricState::Fresh));
    CHECK(a.model().primaryAvailability == ValueAvailability::Live);
    a.apply(primary(1800.0, MetricState::Stale));
    CHECK(a.model().primaryAvailability == ValueAvailability::LastKnown);
    a.apply(primary(0.0, MetricState::Unavailable));
    CHECK(a.model().primaryAvailability == ValueAvailability::Absent);
}

void test_no_fabrication() {
    CoolingInstrumentModelAdapter a;
    a.apply(primary(0.0, MetricState::Unavailable));
    CHECK(a.model().primaryAvailability == ValueAvailability::Absent);
}

void test_negative_clamped() {
    CoolingInstrumentModelAdapter a;
    a.apply(*MetricSample::tryFresh(MetricId::CoolingPrimary, -50.0,
                                    MetricUnit::Rpm, 1));
    CHECK(a.model().primaryRpm >= 0.0);
}

void test_ignores_unrelated() {
    CoolingInstrumentModelAdapter a;
    CHECK(!a.apply(*MetricSample::tryFresh(MetricId::CpuTotalUtilization, 50.0,
                                           MetricUnit::Percent, 1)));
    CHECK(!a.apply(*MetricSample::tryFresh(MetricId::CpuTemperature, 60.0,
                                           MetricUnit::Celsius, 1)));
    CHECK(!a.apply(*MetricSample::tryFresh(MetricId::GpuTotalUtilization, 70.0,
                                           MetricUnit::Percent, 1)));
    CHECK(!a.apply(*MetricSample::tryFresh(MetricId::MemoryUtilization, 30.0,
                                           MetricUnit::Percent, 1)));
    CHECK(!a.apply(*MetricSample::tryFresh(MetricId::MemoryUsedBytes, 1.0e9,
                                           MetricUnit::Bytes, 1)));
    CHECK(a.model().primaryAvailability == ValueAvailability::Absent);
}

void test_initial_absent() {
    CoolingInstrumentModelAdapter a;
    CHECK(a.model().primaryAvailability == ValueAvailability::Absent);
    CHECK(a.model().secondaryTempAvailability == ValueAvailability::Absent);
    CHECK(a.model().secondaryRpmAvailability == ValueAvailability::Absent);
}

}  // namespace

int main() {
    test_primary_rpm();
    test_coolant_secondary();
    test_second_fan_secondary();
    test_state_mapping();
    test_no_fabrication();
    test_negative_clamped();
    test_ignores_unrelated();
    test_initial_absent();
    if (g_failures == 0) {
        std::puts("All CoolingInstrumentModelAdapter tests passed.");
        return 0;
    }
    std::fprintf(stderr, "%d cooling adapter check(s) failed.\n", g_failures);
    return 1;
}
