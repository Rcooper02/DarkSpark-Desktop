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
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QList>
#include <QTemporaryDir>
#include <QObject>

#include "models/MetricSample.hpp"
#include "services/CpuTelemetryService.hpp"
#include "services/CpuThermalService.hpp"
#include "services/HwmonDiscovery.hpp"
#include "services/MemoryTelemetryService.hpp"

using darkspark::models::MetricId;
using darkspark::models::MetricSample;
using darkspark::models::MetricState;
using darkspark::models::MetricUnit;
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

    const QList<MetricSample> samples = service->currentSamples();
    CHECK(samples.size() == 1);
    if (samples.isEmpty()) return;
    const MetricSample s = samples.first();
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
    const QList<MetricSample> samples = service->currentSamples();
    CHECK(samples.size() == 1);
    if (!samples.isEmpty()) {
        CHECK(samples.first() == collector.samples.back());
    }
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
    const QList<MetricSample> samples = service->currentSamples();
    CHECK(samples.size() == 1);
    if (!samples.isEmpty()) {
        CHECK(samples.first().id() == MetricId::CpuTotalUtilization);
    }
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
    const QList<MetricSample> samples = service->currentSamples();
    if (!samples.isEmpty()) {
        CHECK(received.back() == samples.first());
    }
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

    // The service now emits multiple metrics per successful poll (utilization
    // plus the additive used/total byte figures). Tests must locate samples by
    // MetricId rather than by emission order or count. These helpers return the
    // samples for one metric, and the latest such sample, so assertions stay
    // robust to how many other metrics were emitted alongside.
    [[nodiscard]] std::vector<MetricSample> forId(MetricId id) const {
        std::vector<MetricSample> out;
        for (const MetricSample& s : samples) {
            if (s.id() == id) out.push_back(s);
        }
        return out;
    }
    [[nodiscard]] std::optional<MetricSample> latest(MetricId id) const {
        std::optional<MetricSample> found;
        for (const MetricSample& s : samples) {
            if (s.id() == id) found = s;
        }
        return found;
    }
    [[nodiscard]] std::size_t countFor(MetricId id) const {
        return forId(id).size();
    }
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

    // The additive contract: a successful poll emits utilization AND the used/
    // total byte figures. Validate each metric independently, located by
    // MetricId rather than by emission order or total count.
    const auto util = collector.latest(MetricId::MemoryUtilization);
    CHECK(util.has_value());
    if (util) {
        // Unlike CPU, there is no baseline warm-up: the first valid read is
        // usable and Fresh immediately.
        CHECK(util->state() == MetricState::Fresh);
        CHECK(util->unit() == MetricUnit::Percent);
        if (util->value()) CHECK(*util->value() == 60.0);
    }

    // used = (total - available) kB * 1024 = (1000 - 400) * 1024 bytes.
    const auto used = collector.latest(MetricId::MemoryUsedBytes);
    CHECK(used.has_value());
    if (used) {
        CHECK(used->state() == MetricState::Fresh);
        CHECK(used->unit() == MetricUnit::Bytes);
        if (used->value()) CHECK(*used->value() == 600.0 * 1024.0);
    }

    // total = 1000 kB * 1024 bytes.
    const auto total = collector.latest(MetricId::MemoryTotalBytes);
    CHECK(total.has_value());
    if (total) {
        CHECK(total->state() == MetricState::Fresh);
        CHECK(total->unit() == MetricUnit::Bytes);
        if (total->value()) CHECK(*total->value() == 1000.0 * 1024.0);
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

    // Locate the utilization samples specifically; the byte metrics emitted
    // alongside must not affect which sample is the first vs second poll's
    // utilization. Two polls -> two utilization samples, in order.
    const auto utils = collector.forId(MetricId::MemoryUtilization);
    if (utils.size() < 2) { CHECK(false); return; }
    if (utils[0].value()) CHECK(*utils[0].value() == 0.0);
    if (utils[1].value()) CHECK(*utils[1].value() == 100.0);
}

void test_memory_total_missing_is_failure() {
    QObject owner;
    ScriptedMeminfo script;
    script.push(std::string("MemAvailable:   400 kB\n"));
    auto* service = makeMemoryService(script, owner);
    MemoryCollector collector(service);

    driveMemoryPoll(service);

    // A failed poll emits only the utilization metric (no byte samples), as
    // Unavailable. Locate it by id rather than assuming it is samples[0].
    const auto u = collector.latest(MetricId::MemoryUtilization);
    CHECK(u.has_value());
    if (u) CHECK(u->state() == MetricState::Unavailable);
    // No byte samples are emitted on a failed poll.
    CHECK(collector.countFor(MetricId::MemoryUsedBytes) == 0);
    CHECK(collector.countFor(MetricId::MemoryTotalBytes) == 0);
}

void test_memory_available_missing_is_failure_no_fallback() {
    QObject owner;
    ScriptedMeminfo script;
    // MemFree is present but must NOT be used as a substitute.
    script.push(std::string("MemTotal: 1000 kB\nMemFree: 400 kB\n"));
    auto* service = makeMemoryService(script, owner);
    MemoryCollector collector(service);

    driveMemoryPoll(service);

    const auto u = collector.latest(MetricId::MemoryUtilization);
    CHECK(u.has_value());
    if (u) {
        CHECK(u->state() == MetricState::Unavailable);
        CHECK(!u->value().has_value());
    }
}

void test_memory_non_numeric_is_failure() {
    QObject owner;
    ScriptedMeminfo script;
    script.push(std::string("MemTotal: abc kB\nMemAvailable: 400 kB\n"));
    auto* service = makeMemoryService(script, owner);
    MemoryCollector collector(service);

    driveMemoryPoll(service);

    const auto u = collector.latest(MetricId::MemoryUtilization);
    CHECK(u.has_value());
    if (u) CHECK(u->state() == MetricState::Unavailable);
}

void test_memory_zero_total_is_failure() {
    QObject owner;
    ScriptedMeminfo script;
    script.push(meminfo(0, 0));
    auto* service = makeMemoryService(script, owner);
    MemoryCollector collector(service);

    driveMemoryPoll(service);

    const auto u = collector.latest(MetricId::MemoryUtilization);
    CHECK(u.has_value());
    if (u) CHECK(u->state() == MetricState::Unavailable);
}

void test_memory_available_exceeds_total_is_failure() {
    QObject owner;
    ScriptedMeminfo script;
    script.push(meminfo(100, 200));
    auto* service = makeMemoryService(script, owner);
    MemoryCollector collector(service);

    driveMemoryPoll(service);

    const auto u = collector.latest(MetricId::MemoryUtilization);
    CHECK(u.has_value());
    if (u) CHECK(u->state() == MetricState::Unavailable);
    // Impossible input (available > total) must not emit byte samples.
    CHECK(collector.countFor(MetricId::MemoryUsedBytes) == 0);
    CHECK(collector.countFor(MetricId::MemoryTotalBytes) == 0);
}

void test_memory_read_failure_without_prior_value_is_unavailable() {
    QObject owner;
    ScriptedMeminfo script;  // empty -> read returns nullopt
    auto* service = makeMemoryService(script, owner);
    MemoryCollector collector(service);

    driveMemoryPoll(service);

    const auto u = collector.latest(MetricId::MemoryUtilization);
    CHECK(u.has_value());
    if (u) {
        CHECK(u->state() == MetricState::Unavailable);
        CHECK(!u->value().has_value());
    }
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

    // The second poll's utilization must be Stale, retaining the last value.
    // Locate utilization samples by id: the failed poll emits no byte samples,
    // so index-based access would otherwise misalign.
    const auto utils = collector.forId(MetricId::MemoryUtilization);
    if (utils.size() < 2) { CHECK(false); return; }
    CHECK(utils[1].state() == MetricState::Stale);
    if (utils[1].value()) {
        CHECK(*utils[1].value() == 60.0);
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

    // Three polls; the middle is Stale (read failure), the third recovers to
    // Fresh with no Unavailable re-baseline. Located by id so the byte samples
    // emitted on the Fresh polls do not shift indices.
    const auto utils = collector.forId(MetricId::MemoryUtilization);
    if (utils.size() < 3) { CHECK(false); return; }
    CHECK(utils[1].state() == MetricState::Stale);
    // Contrast with CPU: no Unavailable re-baseline step is required.
    CHECK(utils[2].state() == MetricState::Fresh);
    if (utils[2].value()) {
        CHECK(*utils[2].value() == 75.0);
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

    const auto u = collector.latest(MetricId::MemoryUtilization);
    CHECK(u.has_value());
    if (u) {
        CHECK(u->state() == MetricState::Fresh);
        if (u->value()) CHECK(*u->value() == 60.0);
    }
}

void test_memory_large_values_without_overflow() {
    QObject owner;
    ScriptedMeminfo script;
    script.push(meminfo(1000000000000ULL, 250000000000ULL));
    auto* service = makeMemoryService(script, owner);
    MemoryCollector collector(service);

    driveMemoryPoll(service);

    const auto u = collector.latest(MetricId::MemoryUtilization);
    CHECK(u.has_value());
    if (u) {
        CHECK(u->state() == MetricState::Fresh);
        if (u->value()) CHECK(*u->value() == 75.0);
    }
    // The additive byte metrics carry the large values in bytes without
    // overflow: total = 1e12 kB * 1024, used = (1e12 - 250e9) kB * 1024.
    const auto total = collector.latest(MetricId::MemoryTotalBytes);
    CHECK(total.has_value());
    if (total && total->value()) {
        CHECK(*total->value() == 1000000000000.0 * 1024.0);
    }
    const auto used = collector.latest(MetricId::MemoryUsedBytes);
    CHECK(used.has_value());
    if (used && used->value()) {
        CHECK(*used->value() == 750000000000.0 * 1024.0);
    }
}

void test_memory_current_sample_before_start_is_unavailable() {
    QObject owner;
    ScriptedMeminfo script;
    auto* service = makeMemoryService(script, owner);

    const QList<MetricSample> samples = service->currentSamples();
    CHECK(samples.size() == 1);
    if (samples.isEmpty()) return;
    const MetricSample s = samples.first();
    CHECK(s.state() == MetricState::Unavailable);
    CHECK(!s.value().has_value());
    CHECK(s.id() == MetricId::MemoryUtilization);
}

void test_memory_current_samples_include_bytes_after_poll() {
    // The priming contract: once a successful poll has produced the byte
    // figures, currentSamples() exposes all three metrics so the composition
    // root can prime a Memory instrument's full secondary line before the first
    // live signal. Located by id, not by position.
    QObject owner;
    ScriptedMeminfo script;
    script.push(meminfo(1000, 400));
    auto* service = makeMemoryService(script, owner);
    driveMemoryPoll(service);

    const QList<MetricSample> primed = service->currentSamples();
    auto has = [&](MetricId id) {
        for (const MetricSample& s : primed) {
            if (s.id() == id) return true;
        }
        return false;
    };
    CHECK(has(MetricId::MemoryUtilization));
    CHECK(has(MetricId::MemoryUsedBytes));
    CHECK(has(MetricId::MemoryTotalBytes));
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

    const QList<MetricSample> samples = service->currentSamples();
    CHECK(samples.size() == 1);
    if (!samples.isEmpty()) {
        CHECK(samples.first().id() == MetricId::MemoryUtilization);
    }
}

void test_memory_timestamp_carry() {
    QObject owner;
    ScriptedMeminfo script;
    script.push(meminfo(1000, 400));
    auto* service = makeMemoryService(script, owner);
    MemoryCollector collector(service);

    driveMemoryPoll(service);

    // Locate the utilization sample by id; all metrics from one poll share the
    // same monotonic timestamp, which must be carried through as positive.
    const auto u = collector.latest(MetricId::MemoryUtilization);
    CHECK(u.has_value());
    if (u) CHECK(u->timestamp() > 0);
}

// ---------------------------------------------------------------------------
// HwmonDiscovery
//
// All tests build a synthetic hwmon tree under a QTemporaryDir; none touch the
// real /sys/class/hwmon. Kept in this translation unit because the project uses
// a single darkspark-services-test executable.
// ---------------------------------------------------------------------------

using darkspark::services::DiscoveredSensor;
using darkspark::services::HwmonDiscovery;

/// Helper: write `content` to `dir/name`, creating parent dirs as needed.
void writeFile(const QString& dir, const QString& name, const QString& content) {
    QDir().mkpath(dir);
    QFile f(dir + QStringLiteral("/") + name);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        f.write(content.toUtf8());
        f.close();
    }
}

/// Helper: create an hwmon device directory `root/hwmonN` with a name file.
QString makeDevice(const QString& root, int index, const QString& name) {
    const QString dir =
        root + QStringLiteral("/hwmon") + QString::number(index);
    writeFile(dir, QStringLiteral("name"), name);
    return dir;
}

/// Helper: add a temp<slot>_label / temp<slot>_input pair to a device dir.
void addTempPair(const QString& deviceDir, int slot, const QString& label,
                 const QString& milliValue) {
    const QString base = QStringLiteral("temp") + QString::number(slot);
    writeFile(deviceDir, base + QStringLiteral("_label"), label);
    writeFile(deviceDir, base + QStringLiteral("_input"), milliValue);
}

/// Find a discovered sensor by its stable key; returns nullptr if absent.
const DiscoveredSensor* findByKey(const QList<DiscoveredSensor>& list,
                                  const char* key) {
    for (const DiscoveredSensor& s : list) {
        if (s.definition.identity.key == key) {
            return &s;
        }
    }
    return nullptr;
}

void test_discovery_basic_package_and_ccds() {
    QTemporaryDir tmp;
    CHECK(tmp.isValid());
    const QString root = tmp.path();
    const QString dev = makeDevice(root, 3, QStringLiteral("k10temp"));
    addTempPair(dev, 1, QStringLiteral("Tctl"), QStringLiteral("64000"));
    addTempPair(dev, 2, QStringLiteral("Tccd1"), QStringLiteral("60000"));
    addTempPair(dev, 3, QStringLiteral("Tccd2"), QStringLiteral("58000"));

    const HwmonDiscovery discovery(root);
    const QList<DiscoveredSensor> found = discovery.discover();

    CHECK(found.size() == 3);
    const DiscoveredSensor* pkg = findByKey(found, "package");
    const DiscoveredSensor* c1 = findByKey(found, "ccd1");
    const DiscoveredSensor* c2 = findByKey(found, "ccd2");
    CHECK(pkg != nullptr);
    CHECK(c1 != nullptr);
    CHECK(c2 != nullptr);
    if (pkg) {
        CHECK(pkg->definition.category() == MetricId::CpuTemperature);
        CHECK(pkg->definition.unit == darkspark::models::MetricUnit::Celsius);
        CHECK(pkg->definition.displayName == "CPU Package");
        CHECK(pkg->inputPath.endsWith(QStringLiteral("temp1_input")));
    }
    if (c1) CHECK(c1->definition.displayName == "CPU CCD1");
}

void test_discovery_ignores_unrelated_devices() {
    QTemporaryDir tmp;
    CHECK(tmp.isValid());
    const QString root = tmp.path();
    // An unrelated device that must be ignored entirely.
    const QString gpu = makeDevice(root, 0, QStringLiteral("amdgpu"));
    addTempPair(gpu, 1, QStringLiteral("edge"), QStringLiteral("50000"));
    // The CPU device.
    const QString cpu = makeDevice(root, 1, QStringLiteral("k10temp"));
    addTempPair(cpu, 1, QStringLiteral("Tctl"), QStringLiteral("64000"));

    const HwmonDiscovery discovery(root);
    const QList<DiscoveredSensor> found = discovery.discover();

    CHECK(found.size() == 1);
    CHECK(findByKey(found, "package") != nullptr);
}

void test_discovery_missing_name_file_skipped() {
    QTemporaryDir tmp;
    CHECK(tmp.isValid());
    const QString root = tmp.path();
    // Device directory with temp files but no name file.
    const QString dir = root + QStringLiteral("/hwmon0");
    addTempPair(dir, 1, QStringLiteral("Tctl"), QStringLiteral("64000"));

    const HwmonDiscovery discovery(root);
    CHECK(discovery.discover().isEmpty());
}

void test_discovery_missing_paired_input_skipped() {
    QTemporaryDir tmp;
    CHECK(tmp.isValid());
    const QString root = tmp.path();
    const QString dev = makeDevice(root, 0, QStringLiteral("k10temp"));
    // A label with no paired input file.
    writeFile(dev, QStringLiteral("temp1_label"), QStringLiteral("Tctl"));
    // A well-formed one to prove the loop continues past the bad entry.
    addTempPair(dev, 2, QStringLiteral("Tccd1"), QStringLiteral("60000"));

    const HwmonDiscovery discovery(root);
    const QList<DiscoveredSensor> found = discovery.discover();

    CHECK(found.size() == 1);
    CHECK(findByKey(found, "package") == nullptr);
    CHECK(findByKey(found, "ccd1") != nullptr);
}

void test_discovery_malformed_value_skipped() {
    QTemporaryDir tmp;
    CHECK(tmp.isValid());
    const QString root = tmp.path();
    const QString dev = makeDevice(root, 0, QStringLiteral("k10temp"));
    addTempPair(dev, 1, QStringLiteral("Tctl"), QStringLiteral("not-a-number"));
    addTempPair(dev, 2, QStringLiteral("Tccd1"), QStringLiteral("60000"));

    const HwmonDiscovery discovery(root);
    const QList<DiscoveredSensor> found = discovery.discover();

    CHECK(found.size() == 1);
    CHECK(findByKey(found, "package") == nullptr);  // malformed -> skipped
    CHECK(findByKey(found, "ccd1") != nullptr);
}

void test_discovery_missing_label_no_sensor() {
    QTemporaryDir tmp;
    CHECK(tmp.isValid());
    const QString root = tmp.path();
    const QString dev = makeDevice(root, 0, QStringLiteral("k10temp"));
    // An input with no label: nothing to identify it, so nothing is bound.
    writeFile(dev, QStringLiteral("temp1_input"), QStringLiteral("64000"));

    const HwmonDiscovery discovery(root);
    CHECK(discovery.discover().isEmpty());
}

void test_discovery_duplicate_labels_deterministic() {
    QTemporaryDir tmp;
    CHECK(tmp.isValid());
    const QString root = tmp.path();
    const QString dev = makeDevice(root, 0, QStringLiteral("k10temp"));
    // Two labels resolving to the same logical key.
    addTempPair(dev, 1, QStringLiteral("Tccd1"), QStringLiteral("60000"));
    addTempPair(dev, 2, QStringLiteral("Tccd1"), QStringLiteral("61000"));

    const HwmonDiscovery discovery(root);
    const QList<DiscoveredSensor> found = discovery.discover();

    // Exactly one ccd1 is bound; the duplicate is dropped deterministically.
    CHECK(found.size() == 1);
    CHECK(findByKey(found, "ccd1") != nullptr);
}

void test_discovery_tctl_and_tdie_both_present() {
    QTemporaryDir tmp;
    CHECK(tmp.isValid());
    const QString root = tmp.path();
    const QString dev = makeDevice(root, 0, QStringLiteral("k10temp"));
    // Both map to "package": must resolve to a single package sensor.
    addTempPair(dev, 1, QStringLiteral("Tctl"), QStringLiteral("64000"));
    addTempPair(dev, 2, QStringLiteral("Tdie"), QStringLiteral("63000"));

    const HwmonDiscovery discovery(root);
    const QList<DiscoveredSensor> found = discovery.discover();

    CHECK(found.size() == 1);
    CHECK(findByKey(found, "package") != nullptr);
}

void test_discovery_multiple_ccds() {
    QTemporaryDir tmp;
    CHECK(tmp.isValid());
    const QString root = tmp.path();
    const QString dev = makeDevice(root, 0, QStringLiteral("k10temp"));
    addTempPair(dev, 1, QStringLiteral("Tccd1"), QStringLiteral("60000"));
    addTempPair(dev, 2, QStringLiteral("Tccd2"), QStringLiteral("61000"));
    addTempPair(dev, 3, QStringLiteral("Tccd3"), QStringLiteral("62000"));
    addTempPair(dev, 4, QStringLiteral("Tccd4"), QStringLiteral("63000"));

    const HwmonDiscovery discovery(root);
    const QList<DiscoveredSensor> found = discovery.discover();

    CHECK(found.size() == 4);
    CHECK(findByKey(found, "ccd1") != nullptr);
    CHECK(findByKey(found, "ccd4") != nullptr);
    // Deterministic order by key: ccd1, ccd2, ccd3, ccd4.
    if (found.size() == 4) {
        CHECK(found[0].definition.identity.key == "ccd1");
        CHECK(found[3].definition.identity.key == "ccd4");
    }
}

void test_discovery_hwmon_reordering_is_stable() {
    // The same device at a different hwmonN index must yield the same logical
    // sensors, proving discovery does not depend on the number.
    auto build = [](const QString& root, int index) {
        const QString dev = makeDevice(root, index, QStringLiteral("k10temp"));
        addTempPair(dev, 1, QStringLiteral("Tctl"), QStringLiteral("64000"));
        addTempPair(dev, 2, QStringLiteral("Tccd1"), QStringLiteral("60000"));
    };

    QTemporaryDir a;
    QTemporaryDir b;
    CHECK(a.isValid());
    CHECK(b.isValid());
    build(a.path(), 2);
    build(b.path(), 7);

    const QList<DiscoveredSensor> fa = HwmonDiscovery(a.path()).discover();
    const QList<DiscoveredSensor> fb = HwmonDiscovery(b.path()).discover();

    CHECK(fa.size() == 2);
    CHECK(fb.size() == 2);
    // Same logical identities regardless of hwmonN.
    CHECK((findByKey(fa, "package") != nullptr)
          == (findByKey(fb, "package") != nullptr));
    CHECK((findByKey(fa, "ccd1") != nullptr)
          == (findByKey(fb, "ccd1") != nullptr));
}

void test_discovery_sensor_disappearance() {
    // A path discovered earlier may vanish; a re-scan simply omits it, without
    // crashing or reusing the stale path.
    QTemporaryDir tmp;
    CHECK(tmp.isValid());
    const QString root = tmp.path();
    const QString dev = makeDevice(root, 0, QStringLiteral("k10temp"));
    addTempPair(dev, 1, QStringLiteral("Tctl"), QStringLiteral("64000"));
    addTempPair(dev, 2, QStringLiteral("Tccd1"), QStringLiteral("60000"));

    const HwmonDiscovery discovery(root);
    CHECK(discovery.discover().size() == 2);

    // Remove the whole device directory, then re-scan.
    QDir(dev).removeRecursively();
    CHECK(discovery.discover().isEmpty());
}

void test_discovery_empty_root() {
    QTemporaryDir tmp;
    CHECK(tmp.isValid());
    const HwmonDiscovery discovery(tmp.path());
    CHECK(discovery.discover().isEmpty());
}

void test_discovery_nonexistent_root() {
    const HwmonDiscovery discovery(
        QStringLiteral("/does/not/exist/darkspark-test"));
    CHECK(discovery.discover().isEmpty());
}

// ---------------------------------------------------------------------------
// CpuThermalService
//
// Driven through detail::makeWithSources with scripted discovery and reads, so
// no test depends on real hwmon, the real clock, or live temperature. Kept in
// this translation unit (single darkspark-services-test executable).
// ---------------------------------------------------------------------------

using darkspark::services::CpuThermalService;
using darkspark::services::DiscoveredSensor;
using darkspark::services::detail::ThermalSources;

/// Build a DiscoveredSensor with the given key and input path.
DiscoveredSensor makeSensor(const char* key, const QString& inputPath) {
    DiscoveredSensor s;
    s.definition.identity =
        darkspark::models::SensorKey{MetricId::CpuTemperature, key};
    s.definition.displayName = std::string("CPU ") + key;
    s.definition.unit = darkspark::models::MetricUnit::Celsius;
    s.inputPath = inputPath;
    return s;
}

/// Scripted thermal environment: fixed sensor list plus a mutable path->value
/// map so tests can change or remove readings between polls.
class ThermalEnv {
public:
    void addSensor(const char* key, const QString& path, std::optional<double> v) {
        sensors_.append(makeSensor(key, path));
        values_[path] = v;
    }
    void setValue(const QString& path, std::optional<double> v) {
        values_[path] = v;
    }
    [[nodiscard]] QList<DiscoveredSensor> discover() const { return sensors_; }
    [[nodiscard]] std::optional<double> read(const QString& path) const {
        const auto it = values_.find(path);
        if (it == values_.end()) return std::nullopt;
        return it->second;
    }
    [[nodiscard]] MonotonicTimestamp now() { return ++tick_; }

private:
    QList<DiscoveredSensor> sensors_;
    std::map<QString, std::optional<double>> values_;
    MonotonicTimestamp tick_ = 0;
};

CpuThermalService* makeThermal(ThermalEnv& env, QObject& owner) {
    ThermalSources sources;
    sources.discover = [&env]() { return env.discover(); };
    sources.readInput = [&env](const QString& p) { return env.read(p); };
    sources.now = [&env]() { return env.now(); };
    return darkspark::services::detail::makeWithSources(std::move(sources),
                                                        &owner);
}

class ThermalCollector : public QObject {
public:
    explicit ThermalCollector(CpuThermalService* service) {
        connect(service, &CpuThermalService::readingChanged, this,
                [this](const MetricSample& s) { samples.push_back(s); });
    }
    std::vector<MetricSample> samples;
    [[nodiscard]] const MetricSample* last(const char* key) const {
        for (auto it = samples.rbegin(); it != samples.rend(); ++it) {
            if (it->sensorKey() == key) return &*it;
        }
        return nullptr;
    }
};

void driveThermalPoll(CpuThermalService* service) {
    darkspark::services::detail::pollOnceForTest(*service);
}

void test_thermal_normal_reads_multiple_sensors() {
    QObject owner;
    ThermalEnv env;
    env.addSensor("package", QStringLiteral("/p/pkg"), 58.0);
    env.addSensor("ccd1", QStringLiteral("/p/ccd1"), 55.0);
    env.addSensor("ccd2", QStringLiteral("/p/ccd2"), 56.0);
    auto* service = makeThermal(env, owner);
    ThermalCollector collector(service);

    driveThermalPoll(service);

    // One sample per sensor, all Fresh with correct values and identity.
    const MetricSample* pkg = collector.last("package");
    const MetricSample* c1 = collector.last("ccd1");
    const MetricSample* c2 = collector.last("ccd2");
    CHECK(pkg != nullptr);
    CHECK(c1 != nullptr);
    CHECK(c2 != nullptr);
    if (pkg) {
        CHECK(pkg->id() == MetricId::CpuTemperature);
        CHECK(pkg->unit() == darkspark::models::MetricUnit::Celsius);
        CHECK(pkg->state() == MetricState::Fresh);
        CHECK(pkg->value() == std::optional<double>(58.0));
    }
    if (c1 && c1->value()) CHECK(*c1->value() == 55.0);
    if (c2 && c2->value()) CHECK(*c2->value() == 56.0);
}

void test_thermal_no_sensors_discovered() {
    QObject owner;
    ThermalEnv env;  // no sensors
    auto* service = makeThermal(env, owner);
    ThermalCollector collector(service);

    driveThermalPoll(service);

    CHECK(collector.samples.empty());
    CHECK(service->currentSamples().isEmpty());
}

void test_thermal_missing_value_is_unavailable() {
    QObject owner;
    ThermalEnv env;
    // Sensor discovered but its path yields no value.
    env.addSensor("package", QStringLiteral("/p/pkg"), std::nullopt);
    auto* service = makeThermal(env, owner);
    ThermalCollector collector(service);

    driveThermalPoll(service);

    const MetricSample* pkg = collector.last("package");
    CHECK(pkg != nullptr);
    if (pkg) {
        CHECK(pkg->state() == MetricState::Unavailable);
        CHECK(!pkg->value().has_value());
    }
}

void test_thermal_stale_after_valid_then_failure() {
    QObject owner;
    ThermalEnv env;
    env.addSensor("package", QStringLiteral("/p/pkg"), 60.0);
    auto* service = makeThermal(env, owner);
    ThermalCollector collector(service);

    driveThermalPoll(service);                              // Fresh 60
    env.setValue(QStringLiteral("/p/pkg"), std::nullopt);   // now unreadable
    driveThermalPoll(service);                              // Stale 60

    const MetricSample* pkg = collector.last("package");
    CHECK(pkg != nullptr);
    if (pkg) {
        CHECK(pkg->state() == MetricState::Stale);
        CHECK(pkg->value() == std::optional<double>(60.0));
    }
}

void test_thermal_sensor_disappears() {
    QObject owner;
    ThermalEnv env;
    env.addSensor("package", QStringLiteral("/p/pkg"), 58.0);
    env.addSensor("ccd1", QStringLiteral("/p/ccd1"), 55.0);
    auto* service = makeThermal(env, owner);
    ThermalCollector collector(service);

    driveThermalPoll(service);                                // both Fresh
    // ccd1's file disappears (read returns nullopt); package keeps reading.
    env.setValue(QStringLiteral("/p/ccd1"), std::nullopt);
    driveThermalPoll(service);

    const MetricSample* c1 = collector.last("ccd1");
    const MetricSample* pkg = collector.last("package");
    CHECK(c1 != nullptr);
    CHECK(pkg != nullptr);
    if (c1) CHECK(c1->state() == MetricState::Stale);  // had a prior value
    if (pkg) CHECK(pkg->state() == MetricState::Fresh);
}

void test_thermal_malformed_value_is_unavailable() {
    QObject owner;
    ThermalEnv env;
    // A non-finite reading models a malformed conversion; tryFresh rejects it.
    env.addSensor("package", QStringLiteral("/p/pkg"),
                  std::numeric_limits<double>::quiet_NaN());
    auto* service = makeThermal(env, owner);
    ThermalCollector collector(service);

    driveThermalPoll(service);

    const MetricSample* pkg = collector.last("package");
    CHECK(pkg != nullptr);
    if (pkg) {
        CHECK(pkg->state() == MetricState::Unavailable);
        CHECK(!pkg->value().has_value());
    }
}

void test_thermal_recovery_after_failure() {
    QObject owner;
    ThermalEnv env;
    env.addSensor("package", QStringLiteral("/p/pkg"), 60.0);
    auto* service = makeThermal(env, owner);
    ThermalCollector collector(service);

    driveThermalPoll(service);                             // Fresh 60
    env.setValue(QStringLiteral("/p/pkg"), std::nullopt);  // Stale
    driveThermalPoll(service);
    env.setValue(QStringLiteral("/p/pkg"), 62.0);          // recovers
    driveThermalPoll(service);

    const MetricSample* pkg = collector.last("package");
    CHECK(pkg != nullptr);
    if (pkg) {
        CHECK(pkg->state() == MetricState::Fresh);
        CHECK(pkg->value() == std::optional<double>(62.0));
    }
}

void test_thermal_current_samples_track_all_sensors() {
    QObject owner;
    ThermalEnv env;
    env.addSensor("package", QStringLiteral("/p/pkg"), 58.0);
    env.addSensor("ccd1", QStringLiteral("/p/ccd1"), 55.0);
    auto* service = makeThermal(env, owner);

    // Before any poll, discovery has not run: no samples yet.
    CHECK(service->currentSamples().isEmpty());

    driveThermalPoll(service);
    // After a poll, one current sample per discovered sensor.
    CHECK(service->currentSamples().size() == 2);
}

void test_thermal_idempotent_start_stop() {
    QObject owner;
    ThermalEnv env;
    env.addSensor("package", QStringLiteral("/p/pkg"), 58.0);
    auto* service = makeThermal(env, owner);

    service->start();
    service->start();  // no-op
    service->stop();
    service->stop();   // no-op
    // Discovery ran at start; one sensor tracked.
    CHECK(service->currentSamples().size() == 1);
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
    test_memory_current_samples_include_bytes_after_poll();
    test_memory_start_stop_idempotence();
    test_memory_timestamp_carry();

    test_discovery_basic_package_and_ccds();
    test_discovery_ignores_unrelated_devices();
    test_discovery_missing_name_file_skipped();
    test_discovery_missing_paired_input_skipped();
    test_discovery_malformed_value_skipped();
    test_discovery_missing_label_no_sensor();
    test_discovery_duplicate_labels_deterministic();
    test_discovery_tctl_and_tdie_both_present();
    test_discovery_multiple_ccds();
    test_discovery_hwmon_reordering_is_stable();
    test_discovery_sensor_disappearance();
    test_discovery_empty_root();
    test_discovery_nonexistent_root();

    test_thermal_normal_reads_multiple_sensors();
    test_thermal_no_sensors_discovered();
    test_thermal_missing_value_is_unavailable();
    test_thermal_stale_after_valid_then_failure();
    test_thermal_sensor_disappears();
    test_thermal_malformed_value_is_unavailable();
    test_thermal_recovery_after_failure();
    test_thermal_current_samples_track_all_sensors();
    test_thermal_idempotent_start_stop();

    if (g_failures == 0) {
        std::puts("All telemetry service tests passed.");
        return 0;
    }
    std::fprintf(stderr, "%d check(s) failed.\n", g_failures);
    return 1;
}
