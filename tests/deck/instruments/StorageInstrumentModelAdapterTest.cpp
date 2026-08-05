// SPDX-License-Identifier: GPL-3.0-or-later
//
// Deterministic tests for StorageInstrumentModelAdapter. Qt-free.

#include <cmath>
#include <cstdio>

#include "deck/instruments/StorageInstrumentModelAdapter.hpp"
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
bool near(double a, double b) { return std::fabs(a - b) < 1e-3; }

MetricSample fresh(MetricId id, double v, MetricUnit u, const char* key) {
    return *MetricSample::tryFresh(id, v, u, 1, key);
}

void test_joins_all_six() {
    StorageInstrumentModelAdapter a;
    CHECK(a.apply(fresh(MetricId::StorageUtilization, 73.0, MetricUnit::Percent, "fs:/")));
    CHECK(a.apply(fresh(MetricId::StorageUsedBytes, 4.5e11, MetricUnit::Bytes, "fs:/")));
    CHECK(a.apply(fresh(MetricId::StorageTotalBytes, 6.2e11, MetricUnit::Bytes, "fs:/")));
    CHECK(a.apply(fresh(MetricId::StorageTemperature, 41.0, MetricUnit::Celsius, "nvme:x")));
    CHECK(a.apply(fresh(MetricId::StorageReadRate, 1.2e8, MetricUnit::BytesPerSecond, "disk:x")));
    CHECK(a.apply(fresh(MetricId::StorageWriteRate, 5.0e7, MetricUnit::BytesPerSecond, "disk:x")));
    const auto& m = a.model();
    CHECK(near(m.utilizationPercent, 73.0));
    CHECK(near(m.usedBytes, 4.5e11));
    CHECK(near(m.totalBytes, 6.2e11));
    CHECK(near(m.temperatureCelsius, 41.0));
    CHECK(near(m.readBytesPerSec, 1.2e8));
    CHECK(near(m.writeBytesPerSec, 5.0e7));
}

// Temperature and throughput are RETAINED even though the V1 face omits them.
void test_full_model_retention() {
    StorageInstrumentModelAdapter a;
    a.apply(fresh(MetricId::StorageTemperature, 44.0, MetricUnit::Celsius, "nvme:x"));
    a.apply(fresh(MetricId::StorageReadRate, 3.0e8, MetricUnit::BytesPerSecond, "disk:x"));
    a.apply(fresh(MetricId::StorageWriteRate, 1.0e8, MetricUnit::BytesPerSecond, "disk:x"));
    CHECK(a.model().temperatureAvailability == ValueAvailability::Live);
    CHECK(a.model().readAvailability == ValueAvailability::Live);
    CHECK(a.model().writeAvailability == ValueAvailability::Live);
    CHECK(near(a.model().temperatureCelsius, 44.0));
    CHECK(near(a.model().readBytesPerSec, 3.0e8));
    CHECK(near(a.model().writeBytesPerSec, 1.0e8));
}

void test_state_mapping() {
    StorageInstrumentModelAdapter a;
    a.apply(fresh(MetricId::StorageUtilization, 50.0, MetricUnit::Percent, "fs:/"));
    CHECK(a.model().utilizationAvailability == ValueAvailability::Live);
    a.apply(*MetricSample::tryStale(MetricId::StorageUtilization, 50.0, MetricUnit::Percent, 2, "fs:/"));
    CHECK(a.model().utilizationAvailability == ValueAvailability::LastKnown);
    a.apply(MetricSample::unavailable(MetricId::StorageUtilization, 3, "fs:/"));
    CHECK(a.model().utilizationAvailability == ValueAvailability::Absent);
}

void test_no_fabrication() {
    StorageInstrumentModelAdapter a;
    a.apply(MetricSample::unavailable(MetricId::StorageReadRate, 1, "disk:x"));
    CHECK(a.model().readAvailability == ValueAvailability::Absent);
}

void test_clamps() {
    StorageInstrumentModelAdapter a;
    a.apply(fresh(MetricId::StorageUtilization, 250.0, MetricUnit::Percent, "fs:/"));
    CHECK(near(a.model().utilizationPercent, 100.0));
    a.apply(*MetricSample::tryFresh(MetricId::StorageReadRate, -10.0, MetricUnit::BytesPerSecond, 1, "disk:x"));
    CHECK(a.model().readBytesPerSec >= 0.0);
}

void test_ignores_unrelated() {
    StorageInstrumentModelAdapter a;
    CHECK(!a.apply(fresh(MetricId::CpuTotalUtilization, 50.0, MetricUnit::Percent, "")));
    CHECK(!a.apply(fresh(MetricId::GpuTemperature, 60.0, MetricUnit::Celsius, "")));
    CHECK(!a.apply(fresh(MetricId::MemoryUsedBytes, 1e9, MetricUnit::Bytes, "")));
    CHECK(!a.apply(fresh(MetricId::CoolingPrimary, 1800.0, MetricUnit::Rpm, "")));
    CHECK(!a.apply(fresh(MetricId::CoolingCoolantTemp, 34.0, MetricUnit::Celsius, "")));
    CHECK(a.model().utilizationAvailability == ValueAvailability::Absent);
}

void test_initial_absent() {
    StorageInstrumentModelAdapter a;
    CHECK(a.model().utilizationAvailability == ValueAvailability::Absent);
    CHECK(a.model().temperatureAvailability == ValueAvailability::Absent);
    CHECK(a.model().readAvailability == ValueAvailability::Absent);
    CHECK(a.model().writeAvailability == ValueAvailability::Absent);
}

}  // namespace

int main() {
    test_joins_all_six();
    test_full_model_retention();
    test_state_mapping();
    test_no_fabrication();
    test_clamps();
    test_ignores_unrelated();
    test_initial_absent();
    if (g_failures == 0) {
        std::puts("All StorageInstrumentModelAdapter tests passed.");
        return 0;
    }
    std::fprintf(stderr, "%d storage adapter check(s) failed.\n", g_failures);
    return 1;
}
