// SPDX-License-Identifier: GPL-3.0-or-later
#include "models/MetricSample.hpp"
#include "models/SensorDefinition.hpp"
#include <cstdio>
#include <limits>
#include <optional>
using darkspark::models::MetricId;
using darkspark::models::MetricSample;
using darkspark::models::MetricState;
using darkspark::models::MetricUnit;
namespace {
int g_failures = 0;
void reportFail(const char* expr, const char* file, int line) {
    std::fprintf(stderr, "FAIL: %s  (%s:%d)\n", expr, file, line); ++g_failures;
}
#define CHECK(cond) do { if (!(cond)) reportFail(#cond, __FILE__, __LINE__); } while (0)
constexpr MetricId kCpu = MetricId::CpuTotalUtilization;
constexpr MetricUnit kPct = MetricUnit::Percent;
void test_unavailable_has_no_value() {
    const MetricSample s = MetricSample::unavailable(kCpu, 1000);
    CHECK(s.state() == MetricState::Unavailable);
    CHECK(!s.value().has_value());
    CHECK(s.value() != std::optional<double>(0.0));
    CHECK(s.id() == kCpu);
    CHECK(s.timestamp() == 1000);
}
void test_fresh_valid() {
    const auto s = MetricSample::tryFresh(kCpu, 25.0, kPct, 2000);
    CHECK(s.has_value());
    if (!s) return;
    CHECK(s->state() == MetricState::Fresh);
    CHECK(s->value().has_value());
    if (s->value()) CHECK(*s->value() == 25.0);
    CHECK(s->unit() == kPct);
    CHECK(s->timestamp() == 2000);
}
void test_stale_valid() {
    const auto s = MetricSample::tryStale(kCpu, 42.0, kPct, 3000);
    CHECK(s.has_value());
    if (!s) return;
    CHECK(s->state() == MetricState::Stale);
    if (s->value()) CHECK(*s->value() == 42.0);
    CHECK(s->timestamp() == 3000);
}
void test_fresh_boundaries() {
    const auto lo = MetricSample::tryFresh(kCpu, 0.0, kPct, 1);
    const auto hi = MetricSample::tryFresh(kCpu, 100.0, kPct, 2);
    CHECK(lo.has_value()); CHECK(hi.has_value());
    if (lo && lo->value()) CHECK(*lo->value() == 0.0);
    if (hi && hi->value()) CHECK(*hi->value() == 100.0);
}
void test_fresh_nonfinite_returns_nullopt() {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    CHECK(!MetricSample::tryFresh(kCpu, nan, kPct, 1).has_value());
    CHECK(!MetricSample::tryFresh(kCpu, inf, kPct, 1).has_value());
    CHECK(!MetricSample::tryFresh(kCpu, -inf, kPct, 1).has_value());
}
void test_stale_nonfinite_returns_nullopt() {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    CHECK(!MetricSample::tryStale(kCpu, nan, kPct, 1).has_value());
    CHECK(!MetricSample::tryStale(kCpu, inf, kPct, 1).has_value());
    CHECK(!MetricSample::tryStale(kCpu, -inf, kPct, 1).has_value());
}
void test_equality_equal() {
    const auto a = MetricSample::tryFresh(kCpu, 25.0, kPct, 2000);
    const auto b = MetricSample::tryFresh(kCpu, 25.0, kPct, 2000);
    CHECK(a.has_value()); CHECK(b.has_value());
    if (a && b) CHECK(*a == *b);
}
void test_equality_differs_by_value() {
    const auto a = MetricSample::tryFresh(kCpu, 25.0, kPct, 2000);
    const auto b = MetricSample::tryFresh(kCpu, 26.0, kPct, 2000);
    if (a && b) CHECK(*a != *b); else { CHECK(a.has_value()); CHECK(b.has_value()); }
}
void test_equality_differs_by_timestamp() {
    const auto a = MetricSample::tryFresh(kCpu, 25.0, kPct, 2000);
    const auto b = MetricSample::tryFresh(kCpu, 25.0, kPct, 2001);
    if (a && b) CHECK(*a != *b); else { CHECK(a.has_value()); CHECK(b.has_value()); }
}
void test_fresh_not_equal_stale_same_value() {
    const auto f = MetricSample::tryFresh(kCpu, 25.0, kPct, 2000);
    const auto s = MetricSample::tryStale(kCpu, 25.0, kPct, 2000);
    if (f && s) CHECK(*f != *s); else { CHECK(f.has_value()); CHECK(s.has_value()); }
}
void test_timestamp_carry() {
    const MetricSample u = MetricSample::unavailable(kCpu, 111);
    CHECK(u.timestamp() == 111);
    const auto f = MetricSample::tryFresh(kCpu, 1.0, kPct, 222);
    if (f) CHECK(f->timestamp() == 222);
    const auto s = MetricSample::tryStale(kCpu, 1.0, kPct, 333);
    if (s) CHECK(s->timestamp() == 333);
}

// --- T7A.1: composite identity, sensor key, Celsius, SensorDefinition -------

void test_default_sensor_key_is_empty() {
    // Existing single-instance metrics carry an empty key: the category alone
    // identifies them.
    const MetricSample u = MetricSample::unavailable(kCpu, 1);
    CHECK(u.sensorKey().empty());
    const auto f = MetricSample::tryFresh(kCpu, 25.0, kPct, 2);
    if (f) CHECK(f->sensorKey().empty());
}

void test_sensor_key_populated_for_temperature() {
    const auto pkg = MetricSample::tryFresh(MetricId::CpuTemperature, 64.0,
                                            MetricUnit::Celsius, 10, "package");
    CHECK(pkg.has_value());
    if (pkg) {
        CHECK(pkg->id() == MetricId::CpuTemperature);
        CHECK(pkg->sensorKey() == "package");
        CHECK(pkg->unit() == MetricUnit::Celsius);
        CHECK(pkg->value() == std::optional<double>(64.0));
    }
}

void test_identity_composite() {
    const auto ccd1 = MetricSample::tryFresh(MetricId::CpuTemperature, 60.0,
                                             MetricUnit::Celsius, 10, "ccd1");
    if (ccd1) {
        const darkspark::models::SensorKey id = ccd1->identity();
        CHECK(id.category == MetricId::CpuTemperature);
        CHECK(id.key == "ccd1");
    }
}

void test_equality_differs_by_sensor_key() {
    // Same category, unit, value, timestamp, state — different key => different.
    const auto a = MetricSample::tryFresh(MetricId::CpuTemperature, 60.0,
                                          MetricUnit::Celsius, 10, "ccd1");
    const auto b = MetricSample::tryFresh(MetricId::CpuTemperature, 60.0,
                                          MetricUnit::Celsius, 10, "ccd2");
    if (a && b) CHECK(*a != *b);
    else { CHECK(a.has_value()); CHECK(b.has_value()); }
}

void test_equality_same_key_equal() {
    const auto a = MetricSample::tryFresh(MetricId::CpuTemperature, 60.0,
                                          MetricUnit::Celsius, 10, "package");
    const auto b = MetricSample::tryFresh(MetricId::CpuTemperature, 60.0,
                                          MetricUnit::Celsius, 10, "package");
    if (a && b) CHECK(*a == *b);
    else { CHECK(a.has_value()); CHECK(b.has_value()); }
}

void test_sensor_key_struct_equality() {
    using darkspark::models::SensorKey;
    const SensorKey a{MetricId::CpuTemperature, "package"};
    const SensorKey b{MetricId::CpuTemperature, "package"};
    const SensorKey c{MetricId::CpuTemperature, "ccd1"};
    CHECK(a == b);
    CHECK(a != c);
}

void test_celsius_unit_carried() {
    const auto s = MetricSample::tryFresh(MetricId::CpuTemperature, 42.5,
                                          MetricUnit::Celsius, 5, "package");
    if (s) CHECK(s->unit() == MetricUnit::Celsius);
}

void test_sensor_definition_minimal() {
    using darkspark::models::SensorDefinition;
    using darkspark::models::SensorKey;
    SensorDefinition def;
    def.identity = SensorKey{MetricId::CpuTemperature, "package"};
    def.displayName = "CPU Package";
    def.unit = MetricUnit::Celsius;
    CHECK(def.category() == MetricId::CpuTemperature);
    CHECK(def.key() == "package");
    CHECK(def.unit == MetricUnit::Celsius);
    CHECK(def.displayName == "CPU Package");

    SensorDefinition same = def;
    CHECK(same == def);
    same.displayName = "Changed";
    CHECK(same != def);
}
}
int main() {
    test_unavailable_has_no_value();
    test_fresh_valid();
    test_stale_valid();
    test_fresh_boundaries();
    test_fresh_nonfinite_returns_nullopt();
    test_stale_nonfinite_returns_nullopt();
    test_equality_equal();
    test_equality_differs_by_value();
    test_equality_differs_by_timestamp();
    test_fresh_not_equal_stale_same_value();
    test_timestamp_carry();
    test_default_sensor_key_is_empty();
    test_sensor_key_populated_for_temperature();
    test_identity_composite();
    test_equality_differs_by_sensor_key();
    test_equality_same_key_equal();
    test_sensor_key_struct_equality();
    test_celsius_unit_carried();
    test_sensor_definition_minimal();
    if (g_failures == 0) { std::puts("All MetricSample tests passed."); return 0; }
    std::fprintf(stderr, "%d check(s) failed.\n", g_failures); return 1;
}
