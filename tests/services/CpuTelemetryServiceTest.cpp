// SPDX-License-Identifier: GPL-3.0-or-later
//
// Deterministic tests for CpuTelemetryService.
//
// The service is driven through darkspark::services::detail::makeWithSources
// with scripted sources, so no test reads the real /proc/stat, reads the real
// clock, or depends on live CPU load. The QTimer is never started; poll cycles
// are driven by emitting the provider signal path directly through start/stop
// plus scripted reads, so tests do not depend on wall-clock timing.

#include <cstdio>
#include <optional>
#include <string>
#include <vector>

#include <QCoreApplication>
#include <QObject>

#include "models/MetricSample.hpp"
#include "services/CpuTelemetryService.hpp"
#include "services/MemoryTelemetryService.hpp"

using darkspark::models::MetricId;
using darkspark::models::MetricSample;
using darkspark::models::MetricState;
using darkspark::models::MonotonicTimestamp;
using darkspark::services::CpuTelemetryService;
using darkspark::services::detail::TelemetrySources;

namespace {

int g_failures = 0;

void reportFail(const char* expr, const char* file, int line) {
    std::fprintf(stderr, "FAIL: %s  (%s:%d)\n", expr, file, line);
    ++g_failures;
}

#define CHECK(cond) \
    do { if (!(cond)) reportFail(#cond, __FILE__, __LINE__); } while (0)

/// Scripted source: returns queued lines in order, then nullopt.
class ScriptedSource {
public:
    void push(std::optional<std::string> line) { lines_.push_back(line); }

    [[nodiscard]] std::optional<std::string> next() {
        if (index_ >= lines_.size()) {
            return std::nullopt;
        }
        return lines_[index_++];
    }

    [[nodiscard]] MonotonicTimestamp now() { return ++tick_; }

private:
    std::vector<std::optional<std::string>> lines_;
    std::size_t index_ = 0;
    MonotonicTimestamp tick_ = 0;
};

/// Build a service whose reads come from `script`. The service is parented to
/// `owner` so it is destroyed with it.
CpuTelemetryService* makeService(ScriptedSource& script, QObject& owner) {
    TelemetrySources sources;
    sources.readStatLine = [&script]() { return script.next(); };
    sources.now = [&script]() { return script.now(); };
    return darkspark::services::detail::makeWithSources(std::move(sources),
                                                        &owner);
}

/// Collect every sample the service emits.
class Collector : public QObject {
public:
    explicit Collector(CpuTelemetryService* service) {
        connect(service, &CpuTelemetryService::readingChanged, this,
                [this](const MetricSample& s) { samples.push_back(s); });
    }
    std::vector<MetricSample> samples;
};

// A valid aggregate line: user nice system idle iowait irq softirq steal
std::string statLine(unsigned long long user, unsigned long long nice,
                     unsigned long long system, unsigned long long idle,
                     unsigned long long iowait, unsigned long long irq,
                     unsigned long long softirq, unsigned long long steal) {
    return "cpu  " + std::to_string(user) + " " + std::to_string(nice) + " "
           + std::to_string(system) + " " + std::to_string(idle) + " "
           + std::to_string(iowait) + " " + std::to_string(irq) + " "
           + std::to_string(softirq) + " " + std::to_string(steal);
}

/// Drive exactly one sampling cycle synchronously.
///
/// The service polls itself on a QTimer in production; tests must not depend on
/// wall-clock timing or an event loop, so they step the state machine through
/// the internal detail helper instead.
void drivePoll(CpuTelemetryService* service) {
    darkspark::services::detail::pollOnceForTest(*service);
}

// ---------------------------------------------------------------------------

void test_first_valid_read_is_unavailable_baseline() {
    QObject owner;
    ScriptedSource script;
    script.push(statLine(100, 0, 50, 1000, 0, 0, 0, 0));
    auto* service = makeService(script, owner);
    Collector collector(service);

    drivePoll(service);

    CHECK(collector.samples.size() == 1);
    if (collector.samples.empty()) return;
    CHECK(collector.samples[0].state() == MetricState::Unavailable);
    CHECK(!collector.samples[0].value().has_value());
}

void test_second_valid_read_is_fresh_known_value() {
    QObject owner;
    ScriptedSource script;
    // d_total = 200, d_idle = 150 -> 100*(200-150)/200 = 25%
    script.push(statLine(100, 0, 0, 1000, 0, 0, 0, 0));
    script.push(statLine(150, 0, 0, 1150, 0, 0, 0, 0));
    auto* service = makeService(script, owner);
    Collector collector(service);

    drivePoll(service);
    drivePoll(service);

    CHECK(collector.samples.size() == 2);
    if (collector.samples.size() < 2) return;
    CHECK(collector.samples[1].state() == MetricState::Fresh);
    if (collector.samples[1].value()) {
        CHECK(*collector.samples[1].value() == 25.0);
    }
}

void test_idle_only_delta_is_zero_percent() {
    QObject owner;
    ScriptedSource script;
    script.push(statLine(100, 0, 50, 1000, 0, 0, 0, 0));
    script.push(statLine(100, 0, 50, 1100, 0, 0, 0, 0));
    auto* service = makeService(script, owner);
    Collector collector(service);

    drivePoll(service);
    drivePoll(service);

    if (collector.samples.size() < 2) { CHECK(false); return; }
    CHECK(collector.samples[1].state() == MetricState::Fresh);
    if (collector.samples[1].value()) {
        CHECK(*collector.samples[1].value() == 0.0);
    }
}

void test_busy_only_delta_is_hundred_percent() {
    QObject owner;
    ScriptedSource script;
    script.push(statLine(100, 0, 50, 1000, 0, 0, 0, 0));
    script.push(statLine(200, 0, 50, 1000, 0, 0, 0, 0));
    auto* service = makeService(script, owner);
    Collector collector(service);

    drivePoll(service);
    drivePoll(service);

    if (collector.samples.size() < 2) { CHECK(false); return; }
    CHECK(collector.samples[1].state() == MetricState::Fresh);
    if (collector.samples[1].value()) {
        CHECK(*collector.samples[1].value() == 100.0);
    }
}

void test_malformed_line_is_failure() {
    QObject owner;
    ScriptedSource script;
    script.push(std::string("garbage not a cpu line"));
    auto* service = makeService(script, owner);
    Collector collector(service);

    drivePoll(service);

    if (collector.samples.empty()) { CHECK(false); return; }
    CHECK(collector.samples[0].state() == MetricState::Unavailable);
}

void test_too_few_fields_is_failure() {
    QObject owner;
    ScriptedSource script;
    script.push(std::string("cpu  1 2 3"));
    auto* service = makeService(script, owner);
    Collector collector(service);

    drivePoll(service);

    if (collector.samples.empty()) { CHECK(false); return; }
    CHECK(collector.samples[0].state() == MetricState::Unavailable);
}

void test_non_numeric_field_is_failure() {
    QObject owner;
    ScriptedSource script;
    script.push(std::string("cpu  1 2 x 4 5 6 7 8"));
    auto* service = makeService(script, owner);
    Collector collector(service);

    drivePoll(service);

    if (collector.samples.empty()) { CHECK(false); return; }
    CHECK(collector.samples[0].state() == MetricState::Unavailable);
}

void test_wrong_label_is_failure() {
    QObject owner;
    ScriptedSource script;
    script.push(std::string("cpu0 1 2 3 4 5 6 7 8"));
    auto* service = makeService(script, owner);
    Collector collector(service);

    drivePoll(service);

    if (collector.samples.empty()) { CHECK(false); return; }
    CHECK(collector.samples[0].state() == MetricState::Unavailable);
}

void test_read_failure_is_unavailable_without_prior_value() {
    QObject owner;
    ScriptedSource script;  // empty -> read returns nullopt
    auto* service = makeService(script, owner);
    Collector collector(service);

    drivePoll(service);

    if (collector.samples.empty()) { CHECK(false); return; }
    CHECK(collector.samples[0].state() == MetricState::Unavailable);
    CHECK(!collector.samples[0].value().has_value());
}

void test_counter_regression_is_failure() {
    QObject owner;
    ScriptedSource script;
    script.push(statLine(100, 0, 0, 1000, 0, 0, 0, 0));
    script.push(statLine(100, 0, 0, 900, 0, 0, 0, 0));  // idle went backwards
    auto* service = makeService(script, owner);
    Collector collector(service);

    drivePoll(service);
    drivePoll(service);

    if (collector.samples.size() < 2) { CHECK(false); return; }
    CHECK(collector.samples[1].state() == MetricState::Unavailable);
}

void test_zero_total_delta_is_failure() {
    QObject owner;
    ScriptedSource script;
    script.push(statLine(100, 0, 0, 1000, 0, 0, 0, 0));
    script.push(statLine(100, 0, 0, 1000, 0, 0, 0, 0));  // identical
    auto* service = makeService(script, owner);
    Collector collector(service);

    drivePoll(service);
    drivePoll(service);

    if (collector.samples.size() < 2) { CHECK(false); return; }
    CHECK(collector.samples[1].state() == MetricState::Unavailable);
    CHECK(!collector.samples[1].value().has_value());
}

void test_failure_after_valid_value_is_stale() {
    QObject owner;
    ScriptedSource script;
    script.push(statLine(100, 0, 0, 1000, 0, 0, 0, 0));
    script.push(statLine(150, 0, 0, 1150, 0, 0, 0, 0));  // Fresh 25%
    script.push(std::nullopt);                           // read failure
    auto* service = makeService(script, owner);
    Collector collector(service);

    drivePoll(service);
    drivePoll(service);
    drivePoll(service);

    if (collector.samples.size() < 3) { CHECK(false); return; }
    CHECK(collector.samples[2].state() == MetricState::Stale);
    if (collector.samples[2].value()) {
        CHECK(*collector.samples[2].value() == 25.0);
    }
}

void test_recovery_rebaselines_without_cross_gap_delta() {
    QObject owner;
    ScriptedSource script;
    script.push(statLine(100, 0, 0, 1000, 0, 0, 0, 0));   // baseline
    script.push(statLine(150, 0, 0, 1150, 0, 0, 0, 0));   // Fresh 25%
    script.push(std::nullopt);                            // failure -> Stale
    script.push(statLine(500, 0, 0, 5000, 0, 0, 0, 0));   // re-baseline
    script.push(statLine(600, 0, 0, 5100, 0, 0, 0, 0));   // Fresh 50%
    auto* service = makeService(script, owner);
    Collector collector(service);

    for (int i = 0; i < 5; ++i) drivePoll(service);

    if (collector.samples.size() < 5) { CHECK(false); return; }
    CHECK(collector.samples[2].state() == MetricState::Stale);
    // Recovery passes through Unavailable (re-baseline), never a huge delta.
    CHECK(collector.samples[3].state() == MetricState::Unavailable);
    CHECK(collector.samples[4].state() == MetricState::Fresh);
    if (collector.samples[4].value()) {
        // d_total = 200, d_idle = 100 -> 50%
        CHECK(*collector.samples[4].value() == 50.0);
    }
}

void test_guest_fields_not_double_counted() {
    QObject owner;
    ScriptedSource script;
    // Same first eight counters as the 25% case, plus guest/guest_nice which
    // must be ignored (they are already inside user/nice).
    script.push(std::string("cpu  100 0 0 1000 0 0 0 0 999 999"));
    script.push(std::string("cpu  150 0 0 1150 0 0 0 0 999 999"));
    auto* service = makeService(script, owner);
    Collector collector(service);

    drivePoll(service);
    drivePoll(service);

    if (collector.samples.size() < 2) { CHECK(false); return; }
    CHECK(collector.samples[1].state() == MetricState::Fresh);
    if (collector.samples[1].value()) {
        CHECK(*collector.samples[1].value() == 25.0);
    }
}

void test_aggregate_overflow_is_failure() {
    QObject owner;
    ScriptedSource script;
    const std::string maxv = "18446744073709551615";  // uint64 max
    // idle + iowait overflows.
    script.push(std::string("cpu  0 0 0 " + maxv + " " + maxv + " 0 0 0"));
    auto* service = makeService(script, owner);
    Collector collector(service);

    drivePoll(service);

    if (collector.samples.empty()) { CHECK(false); return; }
    CHECK(collector.samples[0].state() == MetricState::Unavailable);
    CHECK(!collector.samples[0].value().has_value());
}

void test_large_counters_without_overflow() {
    QObject owner;
    ScriptedSource script;
    script.push(statLine(1000000000000ULL, 0, 0, 1000000000000ULL, 0, 0, 0, 0));
    script.push(statLine(1000000000100ULL, 0, 0, 1000000000100ULL, 0, 0, 0, 0));
    auto* service = makeService(script, owner);
    Collector collector(service);

    drivePoll(service);
    drivePoll(service);

    if (collector.samples.size() < 2) { CHECK(false); return; }
    CHECK(collector.samples[1].state() == MetricState::Fresh);
    if (collector.samples[1].value()) {
        CHECK(*collector.samples[1].value() == 50.0);
    }
}

void test_current_sample_before_start_is_unavailable() {
    QObject owner;
    ScriptedSource script;
    auto* service = makeService(script, owner);

    const MetricSample s = service->currentSample();
    CHECK(s.state() == MetricState::Unavailable);
    CHECK(!s.value().has_value());
    CHECK(s.id() == MetricId::CpuTotalUtilization);
}

void test_current_sample_matches_last_emitted() {
    QObject owner;
    ScriptedSource script;
    script.push(statLine(100, 0, 0, 1000, 0, 0, 0, 0));
    script.push(statLine(150, 0, 0, 1150, 0, 0, 0, 0));
    auto* service = makeService(script, owner);
    Collector collector(service);

    drivePoll(service);
    drivePoll(service);

    if (collector.samples.empty()) { CHECK(false); return; }
    CHECK(service->currentSample() == collector.samples.back());
}

void test_start_stop_idempotence_and_queryability() {
    QObject owner;
    ScriptedSource script;
    script.push(statLine(100, 0, 0, 1000, 0, 0, 0, 0));
    auto* service = makeService(script, owner);

    service->start();
    service->start();  // no-op
    service->stop();
    service->stop();   // no-op

    // Last emitted sample remains queryable after stop.
    const MetricSample s = service->currentSample();
    CHECK(s.id() == MetricId::CpuTotalUtilization);
}

void test_start_after_stop_clears_baseline() {
    QObject owner;
    ScriptedSource script;
    script.push(statLine(100, 0, 0, 1000, 0, 0, 0, 0));  // baseline
    script.push(statLine(150, 0, 0, 1150, 0, 0, 0, 0));  // Fresh
    script.push(statLine(500, 0, 0, 5000, 0, 0, 0, 0));  // after restart
    auto* service = makeService(script, owner);
    Collector collector(service);

    drivePoll(service);
    drivePoll(service);
    service->start();
    service->stop();
    service->start();  // clears baseline (a measurement gap occurred)
    drivePoll(service);

    if (collector.samples.size() < 3) { CHECK(false); return; }
    // The poll after restart re-baselines rather than deltaing across the gap.
    CHECK(collector.samples[2].state() == MetricState::Unavailable);
}

void test_timestamp_carry() {
    QObject owner;
    ScriptedSource script;
    script.push(statLine(100, 0, 0, 1000, 0, 0, 0, 0));
    auto* service = makeService(script, owner);
    Collector collector(service);

    drivePoll(service);

    if (collector.samples.empty()) { CHECK(false); return; }
    // The scripted clock increments per call; the sample must carry a value
    // produced by that clock, not a real elapsed time.
    CHECK(collector.samples[0].timestamp() > 0);
}

/// Signal-contract test for the wiring T4 depends on: a plain QObject receiver
/// connected to readingChanged must receive every emitted sample, unmodified
/// and in order. This exercises the same connection form the composition root
/// uses, without touching any UI type or production API.
void test_reading_changed_reaches_external_receiver() {
    QObject owner;
    ScriptedSource script;
    script.push(statLine(100, 0, 0, 1000, 0, 0, 0, 0));  // baseline -> Unavailable
    script.push(statLine(150, 0, 0, 1150, 0, 0, 0, 0));  // -> Fresh 25%
    script.push(std::nullopt);                           // -> Stale 25%
    auto* service = makeService(script, owner);

    QObject receiver;
    std::vector<MetricSample> received;
    QObject::connect(service, &CpuTelemetryService::readingChanged, &receiver,
                     [&received](const MetricSample& s) { received.push_back(s); });

    drivePoll(service);
    drivePoll(service);
    drivePoll(service);

    CHECK(received.size() == 3);
    if (received.size() < 3) return;
    CHECK(received[0].state() == MetricState::Unavailable);
    CHECK(received[1].state() == MetricState::Fresh);
    CHECK(received[2].state() == MetricState::Stale);
    if (received[1].value()) CHECK(*received[1].value() == 25.0);
    if (received[2].value()) CHECK(*received[2].value() == 25.0);
    // The receiver sees exactly what the provider reports.
    CHECK(received.back() == service->currentSample());
}

// ---------------------------------------------------------------------------
// MemoryTelemetryService
//
// Kept in this translation unit because the project uses a single
// darkspark-services-test executable; a second test file would introduce a
// second main().
// ---------------------------------------------------------------------------

using darkspark::services::MemoryTelemetryService;
using darkspark::services::detail::MemorySources;

/// Scripted meminfo source: returns queued contents in order, then nullopt.
class ScriptedMeminfo {
public:
    void push(std::optional<std::string> content) { entries_.push_back(content); }

    [[nodiscard]] std::optional<std::string> next() {
        if (index_ >= entries_.size()) {
            return std::nullopt;
        }
        return entries_[index_++];
    }

    [[nodiscard]] MonotonicTimestamp now() { return ++tick_; }

private:
    std::vector<std::optional<std::string>> entries_;
    std::size_t index_ = 0;
    MonotonicTimestamp tick_ = 0;
};

MemoryTelemetryService* makeMemoryService(ScriptedMeminfo& script,
                                          QObject& owner) {
    MemorySources sources;
    sources.readMeminfo = [&script]() { return script.next(); };
    sources.now = [&script]() { return script.now(); };
    return darkspark::services::detail::makeWithSources(std::move(sources),
                                                        &owner);
}

class MemoryCollector : public QObject {
public:
    explicit MemoryCollector(MemoryTelemetryService* service) {
        connect(service, &MemoryTelemetryService::readingChanged, this,
                [this](const MetricSample& s) { samples.push_back(s); });
    }
    std::vector<MetricSample> samples;
};

void driveMemoryPoll(MemoryTelemetryService* service) {
    darkspark::services::detail::pollOnceForTest(*service);
}

std::string meminfo(unsigned long long total, unsigned long long available) {
    return "MemTotal:       " + std::to_string(total)
           + " kB\nMemFree:         12345 kB\nMemAvailable:   "
           + std::to_string(available) + " kB\nBuffers:          6789 kB\n";
}

void test_memory_first_valid_read_is_fresh_immediately() {
    QObject owner;
    ScriptedMeminfo script;
    script.push(meminfo(1000, 400));
    auto* service = makeMemoryService(script, owner);
    MemoryCollector collector(service);

    driveMemoryPoll(service);

    // Unlike CPU, there is no baseline warm-up: the first valid read is usable.
    CHECK(collector.samples.size() == 1);
    if (collector.samples.empty()) return;
    CHECK(collector.samples[0].state() == MetricState::Fresh);
    CHECK(collector.samples[0].id() == MetricId::MemoryUtilization);
    if (collector.samples[0].value()) {
        CHECK(*collector.samples[0].value() == 60.0);
    }
}

void test_memory_boundaries() {
    QObject owner;
    ScriptedMeminfo script;
    script.push(meminfo(1000, 1000));  // fully available -> 0%
    script.push(meminfo(1000, 0));     // none available  -> 100%
    auto* service = makeMemoryService(script, owner);
    MemoryCollector collector(service);

    driveMemoryPoll(service);
    driveMemoryPoll(service);

    if (collector.samples.size() < 2) { CHECK(false); return; }
    if (collector.samples[0].value()) CHECK(*collector.samples[0].value() == 0.0);
    if (collector.samples[1].value()) CHECK(*collector.samples[1].value() == 100.0);
}

void test_memory_total_missing_is_failure() {
    QObject owner;
    ScriptedMeminfo script;
    script.push(std::string("MemAvailable:   400 kB\n"));
    auto* service = makeMemoryService(script, owner);
    MemoryCollector collector(service);

    driveMemoryPoll(service);

    if (collector.samples.empty()) { CHECK(false); return; }
    CHECK(collector.samples[0].state() == MetricState::Unavailable);
}

void test_memory_available_missing_is_failure_no_fallback() {
    QObject owner;
    ScriptedMeminfo script;
    // MemFree is present but must NOT be used as a substitute.
    script.push(std::string("MemTotal: 1000 kB\nMemFree: 400 kB\n"));
    auto* service = makeMemoryService(script, owner);
    MemoryCollector collector(service);

    driveMemoryPoll(service);

    if (collector.samples.empty()) { CHECK(false); return; }
    CHECK(collector.samples[0].state() == MetricState::Unavailable);
    CHECK(!collector.samples[0].value().has_value());
}

void test_memory_non_numeric_is_failure() {
    QObject owner;
    ScriptedMeminfo script;
    script.push(std::string("MemTotal: abc kB\nMemAvailable: 400 kB\n"));
    auto* service = makeMemoryService(script, owner);
    MemoryCollector collector(service);

    driveMemoryPoll(service);

    if (collector.samples.empty()) { CHECK(false); return; }
    CHECK(collector.samples[0].state() == MetricState::Unavailable);
}

void test_memory_zero_total_is_failure() {
    QObject owner;
    ScriptedMeminfo script;
    script.push(meminfo(0, 0));
    auto* service = makeMemoryService(script, owner);
    MemoryCollector collector(service);

    driveMemoryPoll(service);

    if (collector.samples.empty()) { CHECK(false); return; }
    CHECK(collector.samples[0].state() == MetricState::Unavailable);
}

void test_memory_available_exceeds_total_is_failure() {
    QObject owner;
    ScriptedMeminfo script;
    script.push(meminfo(100, 200));
    auto* service = makeMemoryService(script, owner);
    MemoryCollector collector(service);

    driveMemoryPoll(service);

    if (collector.samples.empty()) { CHECK(false); return; }
    CHECK(collector.samples[0].state() == MetricState::Unavailable);
}

void test_memory_read_failure_without_prior_value_is_unavailable() {
    QObject owner;
    ScriptedMeminfo script;  // empty -> read returns nullopt
    auto* service = makeMemoryService(script, owner);
    MemoryCollector collector(service);

    driveMemoryPoll(service);

    if (collector.samples.empty()) { CHECK(false); return; }
    CHECK(collector.samples[0].state() == MetricState::Unavailable);
    CHECK(!collector.samples[0].value().has_value());
}

void test_memory_failure_after_valid_value_is_stale() {
    QObject owner;
    ScriptedMeminfo script;
    script.push(meminfo(1000, 400));  // Fresh 60%
    script.push(std::nullopt);        // read failure
    auto* service = makeMemoryService(script, owner);
    MemoryCollector collector(service);

    driveMemoryPoll(service);
    driveMemoryPoll(service);

    if (collector.samples.size() < 2) { CHECK(false); return; }
    CHECK(collector.samples[1].state() == MetricState::Stale);
    if (collector.samples[1].value()) {
        CHECK(*collector.samples[1].value() == 60.0);
    }
}

void test_memory_recovery_is_fresh_immediately() {
    QObject owner;
    ScriptedMeminfo script;
    script.push(meminfo(1000, 400));  // Fresh 60%
    script.push(std::nullopt);        // Stale
    script.push(meminfo(1000, 250));  // recovery -> Fresh 75%, no re-baseline
    auto* service = makeMemoryService(script, owner);
    MemoryCollector collector(service);

    driveMemoryPoll(service);
    driveMemoryPoll(service);
    driveMemoryPoll(service);

    if (collector.samples.size() < 3) { CHECK(false); return; }
    CHECK(collector.samples[1].state() == MetricState::Stale);
    // Contrast with CPU: no Unavailable re-baseline step is required.
    CHECK(collector.samples[2].state() == MetricState::Fresh);
    if (collector.samples[2].value()) {
        CHECK(*collector.samples[2].value() == 75.0);
    }
}

void test_memory_unknown_keys_ignored() {
    QObject owner;
    ScriptedMeminfo script;
    script.push(std::string("Committed_AS: 999 kB\nMemTotal: 1000 kB\n"
                            "SomethingElse: 5 kB\nMemAvailable: 400 kB\n"
                            "Hugepagesize: 2048 kB\n"));
    auto* service = makeMemoryService(script, owner);
    MemoryCollector collector(service);

    driveMemoryPoll(service);

    if (collector.samples.empty()) { CHECK(false); return; }
    CHECK(collector.samples[0].state() == MetricState::Fresh);
    if (collector.samples[0].value()) {
        CHECK(*collector.samples[0].value() == 60.0);
    }
}

void test_memory_large_values_without_overflow() {
    QObject owner;
    ScriptedMeminfo script;
    script.push(meminfo(1000000000000ULL, 250000000000ULL));
    auto* service = makeMemoryService(script, owner);
    MemoryCollector collector(service);

    driveMemoryPoll(service);

    if (collector.samples.empty()) { CHECK(false); return; }
    CHECK(collector.samples[0].state() == MetricState::Fresh);
    if (collector.samples[0].value()) {
        CHECK(*collector.samples[0].value() == 75.0);
    }
}

void test_memory_current_sample_before_start_is_unavailable() {
    QObject owner;
    ScriptedMeminfo script;
    auto* service = makeMemoryService(script, owner);

    const MetricSample s = service->currentSample();
    CHECK(s.state() == MetricState::Unavailable);
    CHECK(!s.value().has_value());
    CHECK(s.id() == MetricId::MemoryUtilization);
}

void test_memory_start_stop_idempotence() {
    QObject owner;
    ScriptedMeminfo script;
    script.push(meminfo(1000, 400));
    auto* service = makeMemoryService(script, owner);

    service->start();
    service->start();  // no-op
    service->stop();
    service->stop();   // no-op

    const MetricSample s = service->currentSample();
    CHECK(s.id() == MetricId::MemoryUtilization);
}

void test_memory_timestamp_carry() {
    QObject owner;
    ScriptedMeminfo script;
    script.push(meminfo(1000, 400));
    auto* service = makeMemoryService(script, owner);
    MemoryCollector collector(service);

    driveMemoryPoll(service);

    if (collector.samples.empty()) { CHECK(false); return; }
    CHECK(collector.samples[0].timestamp() > 0);
}

}  // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);

    test_first_valid_read_is_unavailable_baseline();
    test_second_valid_read_is_fresh_known_value();
    test_idle_only_delta_is_zero_percent();
    test_busy_only_delta_is_hundred_percent();
    test_malformed_line_is_failure();
    test_too_few_fields_is_failure();
    test_non_numeric_field_is_failure();
    test_wrong_label_is_failure();
    test_read_failure_is_unavailable_without_prior_value();
    test_counter_regression_is_failure();
    test_zero_total_delta_is_failure();
    test_failure_after_valid_value_is_stale();
    test_recovery_rebaselines_without_cross_gap_delta();
    test_guest_fields_not_double_counted();
    test_aggregate_overflow_is_failure();
    test_large_counters_without_overflow();
    test_current_sample_before_start_is_unavailable();
    test_current_sample_matches_last_emitted();
    test_start_stop_idempotence_and_queryability();
    test_start_after_stop_clears_baseline();
    test_timestamp_carry();
    test_reading_changed_reaches_external_receiver();

    test_memory_first_valid_read_is_fresh_immediately();
    test_memory_boundaries();
    test_memory_total_missing_is_failure();
    test_memory_available_missing_is_failure_no_fallback();
    test_memory_non_numeric_is_failure();
    test_memory_zero_total_is_failure();
    test_memory_available_exceeds_total_is_failure();
    test_memory_read_failure_without_prior_value_is_unavailable();
    test_memory_failure_after_valid_value_is_stale();
    test_memory_recovery_is_fresh_immediately();
    test_memory_unknown_keys_ignored();
    test_memory_large_values_without_overflow();
    test_memory_current_sample_before_start_is_unavailable();
    test_memory_start_stop_idempotence();
    test_memory_timestamp_carry();

    if (g_failures == 0) {
        std::puts("All telemetry service tests passed.");
        return 0;
    }
    std::fprintf(stderr, "%d check(s) failed.\n", g_failures);
    return 1;
}
