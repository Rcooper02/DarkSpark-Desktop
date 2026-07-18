// SPDX-License-Identifier: GPL-3.0-or-later
#include "services/CpuTelemetryService.hpp"

#include <array>
#include <charconv>
#include <fstream>
#include <limits>
#include <string_view>
#include <utility>

#include <QLoggingCategory>
#include <QTimer>

namespace darkspark::services {

using models::MetricId;
using models::MetricSample;
using models::MetricState;
using models::MetricUnit;
using models::MonotonicTimestamp;

namespace {

Q_LOGGING_CATEGORY(lcCpuTelemetry, "darkspark.telemetry.cpu")

/// Fixed polling cadence. Deliberately a private constant: there is no
/// settings, configuration file, or global telemetry-configuration object.
constexpr int kPollIntervalMs = 1000;

/// The aggregate /proc/stat line label.
constexpr std::string_view kCpuLabel = "cpu";

/// Number of counters used from the aggregate line:
/// user nice system idle iowait irq softirq steal
/// guest and guest_nice are intentionally excluded; the kernel already includes
/// them in user and nice, so counting them again would double-count guest time.
constexpr std::size_t kUsedFieldCount = 8;

/// Checked addition. Returns false when the addition would overflow, which is
/// treated as an invalid snapshot rather than wrapping silently.
[[nodiscard]] bool checkedAdd(std::uint64_t a, std::uint64_t b,
                              std::uint64_t& out) {
    if (a > std::numeric_limits<std::uint64_t>::max() - b) {
        return false;
    }
    out = a + b;
    return true;
}

/// Read the real aggregate CPU line from /proc/stat. Returns nullopt when the
/// file cannot be opened or has no first line. Never throws for these expected
/// conditions.
[[nodiscard]] std::optional<std::string> readProcStatLine() {
    std::ifstream file("/proc/stat");
    if (!file.is_open()) {
        return std::nullopt;
    }
    std::string line;
    if (!std::getline(file, line)) {
        return std::nullopt;
    }
    return line;
}

/// Parse the eight used counters from an aggregate /proc/stat line.
/// Returns nullopt for any malformed input: wrong label, too few fields,
/// non-numeric fields, or values that do not fit in uint64.
[[nodiscard]] std::optional<std::array<std::uint64_t, kUsedFieldCount>>
parseCounters(std::string_view line) {
    std::size_t pos = line.find_first_not_of(" \t");
    if (pos == std::string_view::npos) {
        return std::nullopt;
    }
    const std::size_t labelEnd = line.find_first_of(" \t", pos);
    if (labelEnd == std::string_view::npos) {
        return std::nullopt;
    }
    // Must be exactly "cpu" (the aggregate line), not "cpu0", "cpu1", etc.
    if (line.substr(pos, labelEnd - pos) != kCpuLabel) {
        return std::nullopt;
    }

    std::array<std::uint64_t, kUsedFieldCount> fields{};
    pos = labelEnd;
    for (std::size_t i = 0; i < kUsedFieldCount; ++i) {
        pos = line.find_first_not_of(" \t", pos);
        if (pos == std::string_view::npos) {
            return std::nullopt;  // fewer than eight counters
        }
        std::size_t end = line.find_first_of(" \t", pos);
        if (end == std::string_view::npos) {
            end = line.size();
        }
        const std::string_view token = line.substr(pos, end - pos);
        std::uint64_t value = 0;
        const char* first = token.data();
        const char* last = token.data() + token.size();
        const auto result = std::from_chars(first, last, value);
        if (result.ec != std::errc{} || result.ptr != last) {
            return std::nullopt;  // non-numeric, signed, or out of range
        }
        fields[i] = value;
        pos = end;
    }
    return fields;
}

}  // namespace

namespace detail {

CpuTelemetryService* makeWithSources(TelemetrySources sources,
                                     QObject* parent) {
    return new CpuTelemetryService(std::move(sources), parent);
}

void pollOnceForTest(CpuTelemetryService& service) { service.poll(); }

}  // namespace detail

CpuTelemetryService::CpuTelemetryService(QObject* parent)
    : CpuTelemetryService(
          detail::TelemetrySources{readProcStatLine, nullptr}, parent) {}

CpuTelemetryService::CpuTelemetryService(detail::TelemetrySources sources,
                                         QObject* parent)
    : ITelemetryProvider(parent), sources_(std::move(sources)),
      timer_(new QTimer(this)),
      current_(MetricSample::unavailable(MetricId::CpuTotalUtilization, 0)) {
    clock_.start();
    timer_->setInterval(kPollIntervalMs);
    connect(timer_, &QTimer::timeout, this, &CpuTelemetryService::poll);
}

CpuTelemetryService::~CpuTelemetryService() { timer_->stop(); }

void CpuTelemetryService::start() {
    if (timer_->isActive()) {
        // Already running: sampling has been continuous, so no measurement gap
        // occurred and the existing baseline still describes a valid interval.
        // Resetting here would inject a spurious Unavailable into a healthy
        // stream.
        return;
    }
    // Starting after a stop: the stopped interval is unmeasured time. A delta
    // spanning it would be reported as Fresh while actually describing the gap,
    // so the baseline is cleared and re-established by the next valid read.
    baseline_.reset();
    timer_->start();
}

void CpuTelemetryService::stop() {
    if (!timer_->isActive()) {
        return;
    }
    timer_->stop();
}

models::MetricSample CpuTelemetryService::currentSample() const {
    return current_;
}

void CpuTelemetryService::emitSample(const MetricSample& sample) {
    const MetricState previous = current_.state();
    current_ = sample;
    if (sample.state() != previous) {
        // State-transition logging only: a persistently unreadable source
        // produces one entry per transition, not one per poll.
        qCInfo(lcCpuTelemetry) << "cpu telemetry state ->"
                               << static_cast<int>(sample.state());
    }
    emit readingChanged(current_);
}

models::MetricSample CpuTelemetryService::makeFailureSample(const char* reason) {
    // Any failure invalidates the baseline so recovery re-baselines instead of
    // computing a delta across the failed interval.
    baseline_.reset();
    const MonotonicTimestamp t = sources_.now ? sources_.now() : clock_.elapsed();

    if (lastValidValue_.has_value()) {
        if (const auto stale = MetricSample::tryStale(
                MetricId::CpuTotalUtilization, *lastValidValue_,
                MetricUnit::Percent, t)) {
            qCDebug(lcCpuTelemetry) << "cpu telemetry failure:" << reason;
            return *stale;
        }
    }
    qCDebug(lcCpuTelemetry) << "cpu telemetry failure:" << reason;
    return MetricSample::unavailable(MetricId::CpuTotalUtilization, t);
}

void CpuTelemetryService::poll() {
    const MonotonicTimestamp t = sources_.now ? sources_.now() : clock_.elapsed();

    const std::optional<std::string> line =
        sources_.readStatLine ? sources_.readStatLine() : std::nullopt;
    if (!line.has_value()) {
        emitSample(makeFailureSample("read failed"));
        return;
    }

    const auto fields = parseCounters(*line);
    if (!fields.has_value()) {
        emitSample(makeFailureSample("parse failed"));
        return;
    }

    // user nice system idle iowait irq softirq steal
    const std::uint64_t user = (*fields)[0];
    const std::uint64_t nice = (*fields)[1];
    const std::uint64_t system = (*fields)[2];
    const std::uint64_t idle = (*fields)[3];
    const std::uint64_t iowait = (*fields)[4];
    const std::uint64_t irq = (*fields)[5];
    const std::uint64_t softirq = (*fields)[6];
    const std::uint64_t steal = (*fields)[7];

    // Every aggregate addition is checked. Aggregation overflow is an invalid
    // snapshot and follows the normal failure path rather than wrapping.
    Snapshot current{};
    if (!checkedAdd(idle, iowait, current.idleAll)) {
        emitSample(makeFailureSample("aggregate overflow"));
        return;
    }
    std::uint64_t busy = 0;
    if (!checkedAdd(user, nice, busy) || !checkedAdd(busy, system, busy)
        || !checkedAdd(busy, irq, busy) || !checkedAdd(busy, softirq, busy)
        || !checkedAdd(busy, steal, busy)) {
        emitSample(makeFailureSample("aggregate overflow"));
        return;
    }
    current.busy = busy;
    if (!checkedAdd(current.idleAll, current.busy, current.total)) {
        emitSample(makeFailureSample("aggregate overflow"));
        return;
    }

    if (!baseline_.has_value()) {
        // First valid read establishes the baseline; no delta is computable yet.
        baseline_ = current;
        emitSample(
            MetricSample::unavailable(MetricId::CpuTotalUtilization, t));
        return;
    }

    const Snapshot& previous = *baseline_;

    // Counter regression is checked before subtracting so unsigned underflow
    // cannot occur.
    if (current.total < previous.total || current.idleAll < previous.idleAll
        || current.busy < previous.busy) {
        emitSample(makeFailureSample("counter regression"));
        return;
    }

    const std::uint64_t deltaTotal = current.total - previous.total;
    const std::uint64_t deltaIdle = current.idleAll - previous.idleAll;
    if (deltaTotal == 0) {
        emitSample(makeFailureSample("zero total delta"));
        return;
    }

    const std::uint64_t deltaBusy = deltaTotal - deltaIdle;
    double percent = 100.0 * static_cast<double>(deltaBusy)
                     / static_cast<double>(deltaTotal);
    // Boundary-only clamp: absorbs negligible floating-point error at the
    // extremes. Invalid input has already been rejected above; this never masks
    // bad data.
    if (percent < 0.0) {
        percent = 0.0;
    } else if (percent > 100.0) {
        percent = 100.0;
    }

    const auto fresh = MetricSample::tryFresh(MetricId::CpuTotalUtilization,
                                              percent, MetricUnit::Percent, t);
    if (!fresh.has_value()) {
        // Unreachable given the gates above, but handled without fabricating a
        // value: treated as a calculation failure, not silently converted.
        emitSample(makeFailureSample("calculation produced no sample"));
        return;
    }

    baseline_ = current;
    lastValidValue_ = percent;
    emitSample(*fresh);
}

}  // namespace darkspark::services
