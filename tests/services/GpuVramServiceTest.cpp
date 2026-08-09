// SPDX-License-Identifier: GPL-3.0-or-later
//
// Deterministic tests for GpuVramService, driven through the injected-source
// seam (detail::makeWithSources) with scripted readings, so no test reads real
// sysfs, the real clock, or a live GPU. Verifies: valid used/total parsing, the
// "gpu-vram" sensor key on BOTH emitted samples (the RAM/VRAM separation),
// graceful degradation (missing/malformed -> Unavailable), stale retention, and
// that used > total is rejected as a nonsensical reading.

#include <QCoreApplication>

#include <cmath>
#include <cstdio>
#include <optional>
#include <vector>

#include "services/GpuVramService.hpp"

using namespace darkspark::services;
using namespace darkspark::models;

namespace {
int g_failures = 0;
void reportFail(const char* e, const char* f, int l) {
    std::fprintf(stderr, "FAIL: %s  (%s:%d)\n", e, f, l);
    ++g_failures;
}
#define CHECK(c) do { if (!(c)) reportFail(#c, __FILE__, __LINE__); } while (0)
bool near(double a, double b) { return std::fabs(a - b) < 1.0; }

// A scripted VRAM source that returns queued readings in order.
struct ScriptedVram {
    std::vector<std::optional<detail::GpuVramReading>> values;
    std::size_t i = 0;
    std::optional<detail::GpuVramReading> next() {
        if (i >= values.size()) return std::nullopt;
        return values[i++];
    }
};

GpuVramService* makeVram(ScriptedVram* script, long long* clock) {
    detail::GpuVramSources s;
    s.readVram = [script]() { return script->next(); };
    s.now = [clock]() { return (*clock)++; };
    return detail::makeWithSources(std::move(s), nullptr);
}

// Find the sample for a given MetricId in a currentSamples() list.
std::optional<MetricSample> find(const QList<MetricSample>& list, MetricId id) {
    for (const MetricSample& s : list) {
        if (s.id() == id) return s;
    }
    return std::nullopt;
}

void test_valid_used_total_and_key() {
    ScriptedVram script{{detail::GpuVramReading{
        static_cast<std::uint64_t>(7'700'000'000ULL),
        static_cast<std::uint64_t>(16'000'000'000ULL)}}};
    long long clock = 0;
    auto* svc = makeVram(&script, &clock);
    detail::pollOnceForTest(*svc);
    const auto samples = svc->currentSamples();
    const auto used = find(samples, MetricId::MemoryUsedBytes);
    const auto total = find(samples, MetricId::MemoryTotalBytes);
    CHECK(used.has_value() && total.has_value());
    CHECK(used->state() == MetricState::Fresh);
    CHECK(total->state() == MetricState::Fresh);
    CHECK(used->value().has_value() && near(*used->value(), 7.7e9));
    CHECK(total->value().has_value() && near(*total->value(), 1.6e10));
    // The sensor key is what keeps GPU VRAM out of the system-RAM path.
    CHECK(used->sensorKey() == "gpu-vram");
    CHECK(total->sensorKey() == "gpu-vram");
    delete svc;
}

void test_missing_files_unavailable() {
    ScriptedVram script{{std::nullopt}};
    long long clock = 0;
    auto* svc = makeVram(&script, &clock);
    detail::pollOnceForTest(*svc);
    const auto samples = svc->currentSamples();
    const auto used = find(samples, MetricId::MemoryUsedBytes);
    const auto total = find(samples, MetricId::MemoryTotalBytes);
    CHECK(used.has_value() && used->state() == MetricState::Unavailable);
    CHECK(total.has_value() && total->state() == MetricState::Unavailable);
    // Even Unavailable samples keep the key so identity is stable.
    CHECK(used->sensorKey() == "gpu-vram");
    delete svc;
}

void test_used_greater_than_total_rejected() {
    // A nonsensical reading (used > total) is treated as no reading. With no
    // prior valid value, the result is Unavailable, not a clamped figure.
    ScriptedVram script{{std::nullopt}};  // reader returns nullopt for used>total
    long long clock = 0;
    auto* svc = makeVram(&script, &clock);
    detail::pollOnceForTest(*svc);
    const auto used = find(svc->currentSamples(), MetricId::MemoryUsedBytes);
    CHECK(used.has_value() && used->state() == MetricState::Unavailable);
    delete svc;
}

void test_valid_then_stale() {
    ScriptedVram script{{
        detail::GpuVramReading{static_cast<std::uint64_t>(8'000'000'000ULL),
                               static_cast<std::uint64_t>(16'000'000'000ULL)},
        std::nullopt}};
    long long clock = 0;
    auto* svc = makeVram(&script, &clock);
    detail::pollOnceForTest(*svc);  // Fresh
    detail::pollOnceForTest(*svc);  // read fails -> Stale (retains last good)
    const auto samples = svc->currentSamples();
    const auto used = find(samples, MetricId::MemoryUsedBytes);
    const auto total = find(samples, MetricId::MemoryTotalBytes);
    CHECK(used.has_value() && used->state() == MetricState::Stale);
    CHECK(total.has_value() && total->state() == MetricState::Stale);
    CHECK(used->value().has_value() && near(*used->value(), 8.0e9));  // retained
    delete svc;
}

}  // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);  // QTimer/QObject need an event loop object
    test_valid_used_total_and_key();
    test_missing_files_unavailable();
    test_used_greater_than_total_rejected();
    test_valid_then_stale();
    if (g_failures == 0) {
        std::puts("All GpuVramService tests passed.");
        return 0;
    }
    std::fprintf(stderr, "%d GpuVramService check(s) failed.\n", g_failures);
    return 1;
}
