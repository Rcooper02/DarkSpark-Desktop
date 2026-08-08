// SPDX-License-Identifier: GPL-3.0-or-later
//
// Deterministic tests for NetworkInstrumentModelAdapter. Qt-free.

#include <cmath>
#include <cstdio>

#include "deck/instruments/NetworkInstrumentModelAdapter.hpp"
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

void test_joins_all_five() {
    NetworkInstrumentModelAdapter a;
    CHECK(a.apply(fresh(MetricId::NetworkReceiveRate, 1.2e8, MetricUnit::BytesPerSecond, "net:eth0")));
    CHECK(a.apply(fresh(MetricId::NetworkTransmitRate, 3.0e7, MetricUnit::BytesPerSecond, "net:eth0")));
    CHECK(a.apply(fresh(MetricId::NetworkReceivedBytes, 9.9e11, MetricUnit::Bytes, "net:eth0")));
    CHECK(a.apply(fresh(MetricId::NetworkTransmittedBytes, 4.4e11, MetricUnit::Bytes, "net:eth0")));
    CHECK(a.apply(fresh(MetricId::NetworkLinkState, 1.0, MetricUnit::Bytes, "net:eth0")));
    const auto& m = a.model();
    CHECK(near(m.receiveBytesPerSec, 1.2e8));
    CHECK(near(m.transmitBytesPerSec, 3.0e7));
    CHECK(near(m.receivedBytes, 9.9e11));
    CHECK(near(m.transmittedBytes, 4.4e11));
    CHECK(m.linkState == NetworkLinkState::Up);
}

// Cumulative bytes + link state are RETAINED even though the V1 face omits them.
void test_retention() {
    NetworkInstrumentModelAdapter a;
    a.apply(fresh(MetricId::NetworkReceivedBytes, 5.0e11, MetricUnit::Bytes, "net:eth0"));
    a.apply(fresh(MetricId::NetworkTransmittedBytes, 2.0e11, MetricUnit::Bytes, "net:eth0"));
    a.apply(fresh(MetricId::NetworkLinkState, 1.0, MetricUnit::Bytes, "net:eth0"));
    CHECK(a.model().receivedAvailability == ValueAvailability::Live);
    CHECK(a.model().transmittedAvailability == ValueAvailability::Live);
    CHECK(a.model().linkAvailability == ValueAvailability::Live);
    CHECK(near(a.model().receivedBytes, 5.0e11));
    CHECK(near(a.model().transmittedBytes, 2.0e11));
}

// Link state retained as numeric 0/1, interpreted by MetricId.
void test_link_state_numeric() {
    NetworkInstrumentModelAdapter a;
    a.apply(fresh(MetricId::NetworkLinkState, 1.0, MetricUnit::Bytes, "net:eth0"));
    CHECK(a.model().linkState == NetworkLinkState::Up);
    a.apply(fresh(MetricId::NetworkLinkState, 0.0, MetricUnit::Bytes, "net:eth0"));
    CHECK(a.model().linkState == NetworkLinkState::Down);
}

void test_state_mapping() {
    NetworkInstrumentModelAdapter a;
    a.apply(fresh(MetricId::NetworkReceiveRate, 100.0, MetricUnit::BytesPerSecond, "net:eth0"));
    CHECK(a.model().receiveAvailability == ValueAvailability::Live);
    a.apply(*MetricSample::tryStale(MetricId::NetworkReceiveRate, 100.0, MetricUnit::BytesPerSecond, 2, "net:eth0"));
    CHECK(a.model().receiveAvailability == ValueAvailability::LastKnown);
    a.apply(MetricSample::unavailable(MetricId::NetworkReceiveRate, 3, "net:eth0"));
    CHECK(a.model().receiveAvailability == ValueAvailability::Absent);
}

void test_no_fabrication() {
    NetworkInstrumentModelAdapter a;
    a.apply(MetricSample::unavailable(MetricId::NetworkReceiveRate, 1, "net:eth0"));
    CHECK(a.model().receiveAvailability == ValueAvailability::Absent);
}

void test_negative_clamped() {
    NetworkInstrumentModelAdapter a;
    a.apply(*MetricSample::tryFresh(MetricId::NetworkReceiveRate, -10.0, MetricUnit::BytesPerSecond, 1, "net:eth0"));
    CHECK(a.model().receiveBytesPerSec >= 0.0);
}

void test_ignores_unrelated() {
    NetworkInstrumentModelAdapter a;
    CHECK(!a.apply(fresh(MetricId::CpuTotalUtilization, 50.0, MetricUnit::Percent, "")));
    CHECK(!a.apply(fresh(MetricId::GpuTemperature, 60.0, MetricUnit::Celsius, "")));
    CHECK(!a.apply(fresh(MetricId::MemoryUsedBytes, 1e9, MetricUnit::Bytes, "")));
    CHECK(!a.apply(fresh(MetricId::CoolingPrimary, 1800.0, MetricUnit::Rpm, "")));
    CHECK(!a.apply(fresh(MetricId::StorageReadRate, 1e8, MetricUnit::BytesPerSecond, "")));
    CHECK(!a.apply(fresh(MetricId::StorageWriteRate, 5e7, MetricUnit::BytesPerSecond, "")));
    CHECK(a.model().receiveAvailability == ValueAvailability::Absent);
}

void test_initial_absent() {
    NetworkInstrumentModelAdapter a;
    CHECK(a.model().receiveAvailability == ValueAvailability::Absent);
    CHECK(a.model().transmitAvailability == ValueAvailability::Absent);
    CHECK(a.model().receivedAvailability == ValueAvailability::Absent);
    CHECK(a.model().transmittedAvailability == ValueAvailability::Absent);
    CHECK(a.model().linkAvailability == ValueAvailability::Absent);
    CHECK(a.model().linkState == NetworkLinkState::Unknown);
}

}  // namespace

int main() {
    test_joins_all_five();
    test_retention();
    test_link_state_numeric();
    test_state_mapping();
    test_no_fabrication();
    test_negative_clamped();
    test_ignores_unrelated();
    test_initial_absent();
    if (g_failures == 0) {
        std::puts("All NetworkInstrumentModelAdapter tests passed.");
        return 0;
    }
    std::fprintf(stderr, "%d network adapter check(s) failed.\n", g_failures);
    return 1;
}
