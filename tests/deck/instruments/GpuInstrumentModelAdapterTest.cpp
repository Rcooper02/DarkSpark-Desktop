// SPDX-License-Identifier: GPL-3.0-or-later
//
// Deterministic tests for the GPU adapter: folding telemetry samples into the
// GPU presentation model. Qt-free and display-free.

#include <cmath>
#include <cstdio>

#include "deck/instruments/GpuInstrumentModelAdapter.hpp"
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
bool near(double a, double b) { return std::fabs(a - b) < 1e-9; }

MetricSample util(double v, MetricState st = MetricState::Fresh) {
    if (st == MetricState::Unavailable)
        return MetricSample::unavailable(MetricId::GpuTotalUtilization, 1);
    if (st == MetricState::Stale)
        return *MetricSample::tryStale(MetricId::GpuTotalUtilization, v,
                                       MetricUnit::Percent, 1);
    return *MetricSample::tryFresh(MetricId::GpuTotalUtilization, v,
                                   MetricUnit::Percent, 1);
}
MetricSample temp(double v, const char* key, MetricState st = MetricState::Fresh) {
    if (st == MetricState::Unavailable)
        return MetricSample::unavailable(MetricId::GpuTemperature, 1, key);
    if (st == MetricState::Stale)
        return *MetricSample::tryStale(MetricId::GpuTemperature, v,
                                       MetricUnit::Celsius, 1, key);
    return *MetricSample::tryFresh(MetricId::GpuTemperature, v,
                                   MetricUnit::Celsius, 1, key);
}

void test_utilization_updates() {
    GpuInstrumentModelAdapter a;
    CHECK(a.apply(util(73.0)));
    CHECK(near(a.model().utilizationPercent, 73.0));
    CHECK(a.model().utilizationAvailability == ValueAvailability::Live);
}
void test_valid_temperature() {
    GpuInstrumentModelAdapter a;
    CHECK(a.apply(temp(54.0, "gpu")));
    CHECK(near(a.model().temperatureCelsius, 54.0));
    CHECK(a.model().temperatureAvailability == ValueAvailability::Live);
}
void test_wrong_sensor_key_ignored() {
    GpuInstrumentModelAdapter a;
    a.apply(temp(54.0, "gpu"));
    CHECK(!a.apply(temp(99.0, "mem")));      // non-primary GPU temp key
    CHECK(!a.apply(temp(88.0, "edge")));     // not the published stable key
    CHECK(near(a.model().temperatureCelsius, 54.0));
}
void test_clamping() {
    GpuInstrumentModelAdapter a;
    a.apply(util(150.0));
    CHECK(near(a.model().utilizationPercent, 100.0));
    a.apply(util(-5.0));
    CHECK(near(a.model().utilizationPercent, 0.0));
}
void test_stale() {
    GpuInstrumentModelAdapter a;
    a.apply(util(40.0));
    CHECK(a.apply(util(40.0, MetricState::Stale)));
    CHECK(a.model().utilizationAvailability == ValueAvailability::LastKnown);
    CHECK(near(a.model().utilizationPercent, 40.0));
}
void test_unavailable_temperature() {
    GpuInstrumentModelAdapter a;
    a.apply(temp(54.0, "gpu"));
    CHECK(a.apply(temp(0.0, "gpu", MetricState::Unavailable)));
    CHECK(a.model().temperatureAvailability == ValueAvailability::Absent);
}
void test_adapter_independence() {
    // CPU and memory samples must never touch the GPU model.
    GpuInstrumentModelAdapter a;
    CHECK(!a.apply(*MetricSample::tryFresh(MetricId::CpuTotalUtilization, 88.0,
                                           MetricUnit::Percent, 1)));
    CHECK(!a.apply(*MetricSample::tryFresh(MetricId::CpuTemperature, 70.0,
                                           MetricUnit::Celsius, 1, "package")));
    CHECK(!a.apply(*MetricSample::tryFresh(MetricId::MemoryUtilization, 60.0,
                                           MetricUnit::Percent, 1)));
    CHECK(a.model().utilizationAvailability == ValueAvailability::Absent);
    CHECK(a.model().temperatureAvailability == ValueAvailability::Absent);
}
void test_initial_absent() {
    GpuInstrumentModelAdapter a;
    CHECK(a.model().utilizationAvailability == ValueAvailability::Absent);
    CHECK(a.model().temperatureAvailability == ValueAvailability::Absent);
}
void test_independent_metric_availability() {
    GpuInstrumentModelAdapter a;
    a.apply(util(33.0));
    CHECK(a.model().utilizationAvailability == ValueAvailability::Live);
    CHECK(a.model().temperatureAvailability == ValueAvailability::Absent);
}

}  // namespace

int main() {
    test_utilization_updates();
    test_valid_temperature();
    test_wrong_sensor_key_ignored();
    test_clamping();
    test_stale();
    test_unavailable_temperature();
    test_adapter_independence();
    test_initial_absent();
    test_independent_metric_availability();
    if (g_failures == 0) {
        std::puts("All GpuInstrumentModelAdapter tests passed.");
        return 0;
    }
    std::fprintf(stderr, "%d GPU adapter check(s) failed.\n", g_failures);
    return 1;
}
