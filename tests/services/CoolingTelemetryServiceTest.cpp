// SPDX-License-Identifier: GPL-3.0-or-later
//
// Deterministic tests for CoolingTelemetryService: provider aggregation, role
// selection over normalized sensors, and Fresh/Stale/Unavailable emission.
// Uses synthetic providers -- no real sysfs. Links Qt6::Core.

#include <QObject>

#include <cstdio>
#include <memory>
#include <optional>
#include <vector>

#include "models/MetricSample.hpp"
#include "services/CoolingSensorProvider.hpp"
#include "services/CoolingTelemetryService.hpp"

using namespace darkspark::services;
using namespace darkspark::services::detail;
using darkspark::models::MetricId;
using darkspark::models::MetricSample;
using darkspark::models::MetricState;
using darkspark::models::MetricUnit;
using darkspark::models::MonotonicTimestamp;

namespace {
int g_failures = 0;
void reportFail(const char* e, const char* f, int l) {
    std::fprintf(stderr, "FAIL: %s  (%s:%d)\n", e, f, l);
    ++g_failures;
}
#define CHECK(c) do { if (!(c)) reportFail(#c, __FILE__, __LINE__); } while (0)

NormalizedCoolingSensor sensor(CoolingSensorRole role, const char* id,
                               std::optional<double> value,
                               CoolingSensorUnit unit = CoolingSensorUnit::Rpm) {
    NormalizedCoolingSensor s;
    s.role = role;
    s.unit = unit;
    s.metadata.stableId = QString::fromUtf8(id);
    s.read = [value]() { return value; };
    return s;
}

// A synthetic provider returning a fixed sensor list.
class FakeProvider : public CoolingSensorProvider {
public:
    FakeProvider(QString name, std::vector<NormalizedCoolingSensor> sensors)
        : name_(std::move(name)), sensors_(std::move(sensors)) {}
    QString providerName() const override { return name_; }
    std::vector<NormalizedCoolingSensor> discover() const override {
        return sensors_;
    }

private:
    QString name_;
    std::vector<NormalizedCoolingSensor> sensors_;
};

AttributedSensor attr(const char* provider, CoolingSensorRole role,
                      const char* id) {
    return AttributedSensor{QString::fromUtf8(provider),
                            sensor(role, id, std::nullopt)};
}

class Collector : public QObject {
public:
    explicit Collector(CoolingTelemetryService* s) {
        connect(s, &darkspark::interfaces::ITelemetryProvider::readingChanged,
                this, [this](const MetricSample& m) { samples.push_back(m); });
    }
    std::vector<MetricSample> samples;
    [[nodiscard]] std::optional<MetricSample> latest(MetricId id) const {
        std::optional<MetricSample> f;
        for (const auto& s : samples)
            if (s.id() == id) f = s;
        return f;
    }
    [[nodiscard]] std::size_t countFor(MetricId id) const {
        std::size_t n = 0;
        for (const auto& s : samples)
            if (s.id() == id) ++n;
        return n;
    }
};

std::vector<CoolingSensorProviderPtr> providers(
    std::vector<NormalizedCoolingSensor> sensors, const char* name = "hwmon") {
    std::vector<CoolingSensorProviderPtr> v;
    v.push_back(std::make_shared<FakeProvider>(QString::fromUtf8(name),
                                               std::move(sensors)));
    return v;
}

// --- Selection rule: highest priority wins, across providers ---------------
void test_selection_prefers_pump() {
    std::vector<AttributedSensor> a{
        attr("hwmon", CoolingSensorRole::CpuFanRpm, "nct:fan1"),
        attr("hwmon", CoolingSensorRole::GpuFanRpm, "amdgpu:fan1"),
        attr("liquidctl", CoolingSensorRole::PumpRpm, "liq:pump"),
    };
    const auto sel = selectFrom(a);
    CHECK(sel.primary.has_value());
    CHECK(sel.primary->sensor.role == CoolingSensorRole::PumpRpm);
}

void test_selection_cpu_over_gpu() {
    std::vector<AttributedSensor> a{
        attr("hwmon", CoolingSensorRole::GpuFanRpm, "amdgpu:fan1"),
        attr("hwmon", CoolingSensorRole::CpuFanRpm, "nct:fan1"),
    };
    const auto sel = selectFrom(a);
    CHECK(sel.primary->sensor.role == CoolingSensorRole::CpuFanRpm);
}

void test_stable_tiebreak() {
    std::vector<AttributedSensor> a{
        attr("hwmon", CoolingSensorRole::CpuFanRpm, "nct:fan2"),
        attr("hwmon", CoolingSensorRole::CpuFanRpm, "nct:fan1"),
    };
    const auto sel = selectFrom(a);
    CHECK(sel.primary->sensor.metadata.stableId == QStringLiteral("nct:fan1"));
}

void test_coolant_secondary() {
    std::vector<AttributedSensor> a{
        attr("hwmon", CoolingSensorRole::PumpRpm, "liq:pump"),
        attr("hwmon", CoolingSensorRole::CoolantTemp, "liq:temp1"),
    };
    const auto sel = selectFrom(a);
    CHECK(sel.secondary.has_value());
    CHECK(sel.secondary->sensor.role == CoolingSensorRole::CoolantTemp);
}

void test_second_fan_secondary() {
    std::vector<AttributedSensor> a{
        attr("hwmon", CoolingSensorRole::CpuFanRpm, "nct:fan1"),
        attr("hwmon", CoolingSensorRole::CaseFanRpm, "nct:fan2"),
    };
    const auto sel = selectFrom(a);
    CHECK(sel.secondary.has_value());
    CHECK(sel.secondary->sensor.metadata.stableId == QStringLiteral("nct:fan2"));
}

void test_no_candidates() {
    const auto sel = selectFrom({});
    CHECK(!sel.primary.has_value());
    CHECK(!sel.secondary.has_value());
}

// --- Service emission ------------------------------------------------------
void test_service_fresh_primary() {
    QObject owner;
    auto* svc = makeWithProviders(
        providers({sensor(CoolingSensorRole::PumpRpm, "liq:pump", 1800.0)}),
        []() { return MonotonicTimestamp(100); }, &owner);
    Collector col(svc);
    pollOnceForTest(*svc);
    const auto p = col.latest(MetricId::CoolingPrimary);
    CHECK(p.has_value());
    if (p) {
        CHECK(p->state() == MetricState::Fresh);
        CHECK(p->unit() == MetricUnit::Rpm);
        if (p->value()) CHECK(*p->value() == 1800.0);
    }
}

void test_service_malformed_unavailable() {
    QObject owner;
    auto* svc = makeWithProviders(
        providers({sensor(CoolingSensorRole::CpuFanRpm, "nct:fan1",
                          std::nullopt)}),
        []() { return MonotonicTimestamp(100); }, &owner);
    Collector col(svc);
    pollOnceForTest(*svc);
    const auto p = col.latest(MetricId::CoolingPrimary);
    CHECK(p.has_value());
    if (p) {
        CHECK(p->state() == MetricState::Unavailable);
        CHECK(!p->value().has_value());
    }
}

void test_service_no_sensors_unavailable() {
    QObject owner;
    auto* svc = makeWithProviders(providers({}),
                                  []() { return MonotonicTimestamp(100); },
                                  &owner);
    Collector col(svc);
    pollOnceForTest(*svc);
    const auto p = col.latest(MetricId::CoolingPrimary);
    CHECK(p.has_value());
    if (p) CHECK(p->state() == MetricState::Unavailable);
}

void test_service_coolant_secondary_celsius() {
    QObject owner;
    auto* svc = makeWithProviders(
        providers({sensor(CoolingSensorRole::PumpRpm, "liq:pump", 1800.0),
                   sensor(CoolingSensorRole::CoolantTemp, "liq:temp1", 34.0,
                          CoolingSensorUnit::Celsius)}),
        []() { return MonotonicTimestamp(100); }, &owner);
    Collector col(svc);
    pollOnceForTest(*svc);
    const auto t = col.latest(MetricId::CoolingCoolantTemp);
    CHECK(t.has_value());
    if (t) {
        CHECK(t->unit() == MetricUnit::Celsius);
        if (t->value()) CHECK(*t->value() == 34.0);
    }
    // No CoolingSecondary fan sample when a coolant temp is the secondary.
    CHECK(col.countFor(MetricId::CoolingSecondary) == 0);
}

void test_service_aggregates_multiple_providers() {
    QObject owner;
    std::vector<CoolingSensorProviderPtr> provs;
    provs.push_back(std::make_shared<FakeProvider>(
        QStringLiteral("hwmon"),
        std::vector<NormalizedCoolingSensor>{
            sensor(CoolingSensorRole::CpuFanRpm, "nct:fan1", 1200.0)}));
    provs.push_back(std::make_shared<FakeProvider>(
        QStringLiteral("liquidctl"),
        std::vector<NormalizedCoolingSensor>{
            sensor(CoolingSensorRole::PumpRpm, "liq:pump", 1800.0)}));
    auto* svc = makeWithProviders(std::move(provs),
                                  []() { return MonotonicTimestamp(100); },
                                  &owner);
    Collector col(svc);
    pollOnceForTest(*svc);
    // Pump (from the liquidctl provider) wins primary across providers.
    const auto p = col.latest(MetricId::CoolingPrimary);
    CHECK(p.has_value());
    if (p && p->value()) CHECK(*p->value() == 1800.0);
}

}  // namespace

int main() {
    test_selection_prefers_pump();
    test_selection_cpu_over_gpu();
    test_stable_tiebreak();
    test_coolant_secondary();
    test_second_fan_secondary();
    test_no_candidates();
    test_service_fresh_primary();
    test_service_malformed_unavailable();
    test_service_no_sensors_unavailable();
    test_service_coolant_secondary_celsius();
    test_service_aggregates_multiple_providers();
    if (g_failures == 0) {
        std::puts("All CoolingTelemetryService tests passed.");
        return 0;
    }
    std::fprintf(stderr, "%d cooling service check(s) failed.\n", g_failures);
    return 1;
}
