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

MetricSample vram(MetricId id, double v, const char* key,
                  MetricState st = MetricState::Fresh) {
    if (st == MetricState::Unavailable)
        return MetricSample::unavailable(id, 1, key);
    if (st == MetricState::Stale)
        return *MetricSample::tryStale(id, v, MetricUnit::Bytes, 1, key);
    return *MetricSample::tryFresh(id, v, MetricUnit::Bytes, 1, key);
}

void test_vram_updates_with_key() {
    GpuInstrumentModelAdapter a;
    CHECK(a.apply(vram(MetricId::MemoryUsedBytes, 7.7e9, "gpu-vram")));
    CHECK(a.apply(vram(MetricId::MemoryTotalBytes, 1.6e10, "gpu-vram")));
    CHECK(near(a.model().vramUsedBytes, 7.7e9));
    CHECK(near(a.model().vramTotalBytes, 1.6e10));
    CHECK(a.model().vramAvailability == ValueAvailability::Live);
}
void test_keyless_memory_is_system_ram_ignored() {
    // MemoryUsedBytes/MemoryTotalBytes with NO key are SYSTEM RAM: the GPU
    // adapter must ignore them so RAM never leaks into the VRAM line.
    GpuInstrumentModelAdapter a;
    CHECK(!a.apply(vram(MetricId::MemoryUsedBytes, 4.0e9, "")));
    CHECK(!a.apply(vram(MetricId::MemoryTotalBytes, 3.2e10, "")));
    CHECK(a.model().vramAvailability == ValueAvailability::Absent);
    CHECK(near(a.model().vramUsedBytes, 0.0));
}
void test_vram_unavailable_leaves_util_temp() {
    GpuInstrumentModelAdapter a;
    a.apply(util(50.0));
    a.apply(temp(70.0, "gpu"));
    a.apply(vram(MetricId::MemoryUsedBytes, 0.0, "gpu-vram",
                 MetricState::Unavailable));
    CHECK(a.model().vramAvailability == ValueAvailability::Absent);
    // Utilization and temperature are undisturbed.
    CHECK(near(a.model().utilizationPercent, 50.0));
    CHECK(a.model().utilizationAvailability == ValueAvailability::Live);
    CHECK(near(a.model().temperatureCelsius, 70.0));
    CHECK(a.model().temperatureAvailability == ValueAvailability::Live);
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
    test_vram_updates_with_key();
    test_keyless_memory_is_system_ram_ignored();
    test_vram_unavailable_leaves_util_temp();
    if (g_failures == 0) {
        std::puts("All GpuInstrumentModelAdapter tests passed.");
        return 0;
    }
    std::fprintf(stderr, "%d GPU adapter check(s) failed.\n", g_failures);
    return 1;
}
