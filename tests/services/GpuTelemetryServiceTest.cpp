// SPDX-License-Identifier: GPL-3.0-or-later
//
// Deterministic tests for the GPU telemetry services, driven through the
// injected-source seams (detail::makeWithSources) with scripted sources, so no
// test reads real sysfs, the real clock, or a live GPU.

#include <QCoreApplication>

#include <cmath>
#include <cstdio>
#include <optional>

#include "services/GpuTelemetryService.hpp"
#include "services/GpuThermalService.hpp"

using namespace darkspark::services;
using namespace darkspark::models;

namespace {
int g_failures = 0;
void reportFail(const char* e, const char* f, int l) {
    std::fprintf(stderr, "FAIL: %s  (%s:%d)\n", e, f, l);
    ++g_failures;
}
#define CHECK(c) do { if (!(c)) reportFail(#c, __FILE__, __LINE__); } while (0)
bool near(double a, double b) { return std::fabs(a - b) < 1e-6; }

// ---- Utilization service ---------------------------------------------------

// A scripted busy-percent source that returns queued values in order.
struct ScriptedBusy {
    std::vector<std::optional<std::string>> values;
    std::size_t i = 0;
    std::optional<std::string> next() {
        if (i >= values.size()) return std::nullopt;
        return values[i++];
    }
};

GpuTelemetryService* makeUtil(ScriptedBusy* script, long long* clock) {
    detail::GpuUtilizationSources s;
    s.readBusyPercent = [script]() { return script->next(); };
    s.now = [clock]() { return (*clock)++; };
    return detail::makeWithSources(std::move(s), nullptr);
}

void test_util_valid_parsing() {
    ScriptedBusy script{{std::optional<std::string>("73\n")}};
    long long clock = 0;
    auto* svc = makeUtil(&script, &clock);
    detail::pollOnceForTest(*svc);
    const auto s = svc->currentSamples().first();
    CHECK(s.state() == MetricState::Fresh);
    CHECK(s.value().has_value() && near(*s.value(), 73.0));
    delete svc;
}

void test_util_missing_device_unavailable() {
    // Source always returns nullopt (no amdgpu device / unreadable).
    ScriptedBusy script{{std::nullopt, std::nullopt}};
    long long clock = 0;
    auto* svc = makeUtil(&script, &clock);
    detail::pollOnceForTest(*svc);
    const auto s = svc->currentSamples().first();
    CHECK(s.state() == MetricState::Unavailable);
    CHECK(!s.value().has_value());
    delete svc;
}

void test_util_malformed_then_stale() {
    // First a valid read, then malformed -> should go Stale (retain last value).
    ScriptedBusy script{{std::optional<std::string>("50\n"),
                         std::optional<std::string>("garbage")}};
    long long clock = 0;
    auto* svc = makeUtil(&script, &clock);
    detail::pollOnceForTest(*svc);
    CHECK(svc->currentSamples().first().state() == MetricState::Fresh);
    detail::pollOnceForTest(*svc);
    const auto s = svc->currentSamples().first();
    CHECK(s.state() == MetricState::Stale);
    CHECK(s.value().has_value() && near(*s.value(), 50.0));  // retained
    delete svc;
}

void test_util_out_of_range_is_malformed() {
    // 250 is outside the driver's [0,100]; treated as malformed -> Unavailable
    // (no prior valid value to fall back to).
    ScriptedBusy script{{std::optional<std::string>("250\n")}};
    long long clock = 0;
    auto* svc = makeUtil(&script, &clock);
    detail::pollOnceForTest(*svc);
    CHECK(svc->currentSamples().first().state() == MetricState::Unavailable);
    delete svc;
}

// ---- Thermal service -------------------------------------------------------

GpuThermalService* makeThermal(
    std::function<std::optional<QString>()> discover,
    std::function<std::optional<double>(const QString&)> read,
    long long* clock) {
    detail::GpuThermalSources s;
    s.discoverInputPath = std::move(discover);
    s.readMilliCelsius = std::move(read);
    s.now = [clock]() { return (*clock)++; };
    return detail::makeWithSources(std::move(s), nullptr);
}

void test_thermal_valid() {
    long long clock = 0;
    auto* svc = makeThermal(
        []() { return std::optional<QString>(QStringLiteral("/fake/temp1_input")); },
        [](const QString&) { return std::optional<double>(54.0); }, &clock);
    detail::pollOnceForTest(*svc);
    const auto s = svc->currentSamples().first();
    CHECK(s.state() == MetricState::Fresh);
    CHECK(s.value().has_value() && near(*s.value(), 54.0));
    CHECK(s.sensorKey() == "gpu");  // stable published key
    delete svc;
}

void test_thermal_missing_device() {
    long long clock = 0;
    auto* svc = makeThermal([]() { return std::optional<QString>(std::nullopt); },
                            [](const QString&) { return std::optional<double>(); },
                            &clock);
    detail::pollOnceForTest(*svc);
    CHECK(svc->currentSamples().first().state() == MetricState::Unavailable);
    delete svc;
}

void test_thermal_malformed_then_stale() {
    long long clock = 0;
    bool firstRead = true;
    auto* svc = makeThermal(
        []() { return std::optional<QString>(QStringLiteral("/fake/temp1_input")); },
        [&firstRead](const QString&) -> std::optional<double> {
            if (firstRead) { firstRead = false; return 60.0; }
            return std::nullopt;  // malformed/unreadable on second poll
        },
        &clock);
    detail::pollOnceForTest(*svc);
    CHECK(svc->currentSamples().first().state() == MetricState::Fresh);
    detail::pollOnceForTest(*svc);
    const auto s = svc->currentSamples().first();
    CHECK(s.state() == MetricState::Stale);
    CHECK(s.value().has_value() && near(*s.value(), 60.0));  // retained
    delete svc;
}

}  // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);  // QTimer/QObject need an event loop object
    test_util_valid_parsing();
    test_util_missing_device_unavailable();
    test_util_malformed_then_stale();
    test_util_out_of_range_is_malformed();
    test_thermal_valid();
    test_thermal_missing_device();
    test_thermal_malformed_then_stale();
    if (g_failures == 0) {
        std::puts("All GpuTelemetryService tests passed.");
        return 0;
    }
    std::fprintf(stderr, "%d GPU service check(s) failed.\n", g_failures);
    return 1;
}
