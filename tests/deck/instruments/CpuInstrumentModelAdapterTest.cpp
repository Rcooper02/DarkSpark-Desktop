// SPDX-License-Identifier: GPL-3.0-or-later
//
// Deterministic tests for the non-visual live-integration logic: the adapter
// that folds telemetry samples into the CpuInstrument presentation model.
// Qt-free and display-free.

#include <cmath>
#include <cstdio>

#include "deck/instruments/CpuInstrumentModelAdapter.hpp"
#include "models/MetricSample.hpp"

using namespace darkspark::deck::instruments;
using namespace darkspark::models;

namespace {
int g_failures = 0;
void reportFail(const char* expr, const char* file, int line) {
    std::fprintf(stderr, "FAIL: %s  (%s:%d)\n", expr, file, line);
    ++g_failures;
}
#define CHECK(cond) \
    do { if (!(cond)) reportFail(#cond, __FILE__, __LINE__); } while (0)

bool near(double a, double b) { return std::fabs(a - b) < 1e-9; }

MetricSample util(double v, MetricState st = MetricState::Fresh) {
    if (st == MetricState::Unavailable) {
        return MetricSample::unavailable(MetricId::CpuTotalUtilization, 1);
    }
    if (st == MetricState::Stale) {
        return *MetricSample::tryStale(MetricId::CpuTotalUtilization, v,
                                       MetricUnit::Percent, 1);
    }
    return *MetricSample::tryFresh(MetricId::CpuTotalUtilization, v,
                                   MetricUnit::Percent, 1);
}
MetricSample temp(double v, const char* key,
                  MetricState st = MetricState::Fresh) {
    if (st == MetricState::Unavailable) {
        return MetricSample::unavailable(MetricId::CpuTemperature, 1, key);
    }
    if (st == MetricState::Stale) {
        return *MetricSample::tryStale(MetricId::CpuTemperature, v,
                                       MetricUnit::Celsius, 1, key);
    }
    return *MetricSample::tryFresh(MetricId::CpuTemperature, v,
                                   MetricUnit::Celsius, 1, key);
}

void test_utilization_updates_model() {
    CpuInstrumentModelAdapter a;
    CHECK(a.apply(util(42.0)));
    CHECK(near(a.model().utilizationPercent, 42.0));
    CHECK(a.model().utilizationAvailability == ValueAvailability::Live);
}

void test_package_temperature_updates_model() {
    CpuInstrumentModelAdapter a;
    CHECK(a.apply(temp(61.0, "package")));
    CHECK(near(a.model().temperatureCelsius, 61.0));
    CHECK(a.model().temperatureAvailability == ValueAvailability::Live);
}

void test_ccd_sample_does_not_replace_package() {
    CpuInstrumentModelAdapter a;
    a.apply(temp(61.0, "package"));
    // A CCD sample must be ignored and must not change the package temperature.
    CHECK(!a.apply(temp(70.0, "ccd1")));
    CHECK(!a.apply(temp(72.0, "ccd2")));
    CHECK(near(a.model().temperatureCelsius, 61.0));
    CHECK(a.model().temperatureAvailability == ValueAvailability::Live);
}

void test_stale_utilization_keeps_value_marks_lastknown() {
    CpuInstrumentModelAdapter a;
    a.apply(util(50.0));
    CHECK(a.apply(util(50.0, MetricState::Stale)));
    CHECK(a.model().utilizationAvailability == ValueAvailability::LastKnown);
    CHECK(near(a.model().utilizationPercent, 50.0));
}

void test_unavailable_package_temperature_is_absent() {
    CpuInstrumentModelAdapter a;
    a.apply(temp(61.0, "package"));
    CHECK(a.apply(temp(0.0, "package", MetricState::Unavailable)));
    CHECK(a.model().temperatureAvailability == ValueAvailability::Absent);
}

void test_clamps_out_of_range_utilization() {
    CpuInstrumentModelAdapter a;
    a.apply(util(150.0));
    CHECK(near(a.model().utilizationPercent, 100.0));
    a.apply(util(-20.0));
    CHECK(near(a.model().utilizationPercent, 0.0));
}

void test_memory_sample_ignored() {
    CpuInstrumentModelAdapter a;
    const auto mem = MetricSample::tryFresh(MetricId::MemoryUtilization, 80.0,
                                            MetricUnit::Percent, 1);
    CHECK(mem.has_value());
    CHECK(!a.apply(*mem));
    CHECK(a.model().utilizationAvailability == ValueAvailability::Absent);
}

void test_initial_state_all_absent() {
    CpuInstrumentModelAdapter a;
    CHECK(a.model().utilizationAvailability == ValueAvailability::Absent);
    CHECK(a.model().temperatureAvailability == ValueAvailability::Absent);
}

void test_independent_metric_availability() {
    // Utilization present, package temperature absent (no sensor): the two
    // availabilities are independent.
    CpuInstrumentModelAdapter a;
    a.apply(util(33.0));
    CHECK(a.model().utilizationAvailability == ValueAvailability::Live);
    CHECK(a.model().temperatureAvailability == ValueAvailability::Absent);
}

void test_same_model_feeds_both_sizes() {
    // The preview fans one adapter model to Small and Large. Model equality here
    // is what guarantees they stay synchronized: two reads of the same adapter
    // model yield identical logical values.
    CpuInstrumentModelAdapter a;
    a.apply(util(42.0));
    a.apply(temp(61.0, "package"));
    const CpuInstrumentModel m1 = a.model();
    const CpuInstrumentModel m2 = a.model();
    CHECK(near(m1.utilizationPercent, m2.utilizationPercent));
    CHECK(near(m1.temperatureCelsius, m2.temperatureCelsius));
    CHECK(m1.utilizationAvailability == m2.utilizationAvailability);
    CHECK(m1.temperatureAvailability == m2.temperatureAvailability);
    CHECK(near(m1.utilizationPercent, 42.0));
    CHECK(near(m1.temperatureCelsius, 61.0));
}

void test_reset_returns_to_absent() {
    CpuInstrumentModelAdapter a;
    a.apply(util(42.0));
    a.reset();
    CHECK(a.model().utilizationAvailability == ValueAvailability::Absent);
}

}  // namespace

int main() {
    test_utilization_updates_model();
    test_package_temperature_updates_model();
    test_ccd_sample_does_not_replace_package();
    test_stale_utilization_keeps_value_marks_lastknown();
    test_unavailable_package_temperature_is_absent();
    test_clamps_out_of_range_utilization();
    test_memory_sample_ignored();
    test_initial_state_all_absent();
    test_independent_metric_availability();
    test_same_model_feeds_both_sizes();
    test_reset_returns_to_absent();

    if (g_failures == 0) {
        std::puts("All CpuInstrumentModelAdapter tests passed.");
        return 0;
    }
    std::fprintf(stderr, "%d check(s) failed.\n", g_failures);
    return 1;
}
