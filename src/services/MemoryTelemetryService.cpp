// SPDX-License-Identifier: GPL-3.0-or-later
#include "services/MemoryTelemetryService.hpp"

#include <charconv>
#include <fstream>
#include <sstream>
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

Q_LOGGING_CATEGORY(lcMemoryTelemetry, "darkspark.telemetry.memory")

/// Fixed polling cadence. Deliberately a private constant: there is no settings,
/// configuration file, or global telemetry-configuration object.
constexpr int kPollIntervalMs = 1000;

constexpr std::string_view kMemTotalKey = "MemTotal";
constexpr std::string_view kMemAvailableKey = "MemAvailable";

/// Read the whole of /proc/meminfo. Returns nullopt when the file cannot be
/// opened. Never throws for these expected conditions.
[[nodiscard]] std::optional<std::string> readProcMeminfo() {
    std::ifstream file("/proc/meminfo");
    if (!file.is_open()) {
        return std::nullopt;
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    if (file.bad()) {
        return std::nullopt;
    }
    return buffer.str();
}

/// Extract the unsigned value for `key` from meminfo content.
///
/// Lines have the form "Key:<whitespace><number>[ kB]". Returns nullopt when the
/// key is absent, the value is missing, non-numeric, or does not fit in uint64.
[[nodiscard]] std::optional<std::uint64_t> valueForKey(std::string_view content,
                                                       std::string_view key) {
    std::size_t pos = 0;
    while (pos < content.size()) {
        std::size_t lineEnd = content.find('\n', pos);
        const bool lastLine = (lineEnd == std::string_view::npos);
        if (lastLine) {
            lineEnd = content.size();
        }
        const std::string_view line = content.substr(pos, lineEnd - pos);
        pos = lastLine ? content.size() : lineEnd + 1;

        const std::size_t colon = line.find(':');
        if (colon == std::string_view::npos) {
            continue;
        }
        if (line.substr(0, colon) != key) {
            continue;
        }

        std::string_view rest = line.substr(colon + 1);
        const std::size_t first = rest.find_first_not_of(" \t");
        if (first == std::string_view::npos) {
            return std::nullopt;  // key present but no value
        }
        rest = rest.substr(first);
        std::size_t end = rest.find_first_of(" \t");
        if (end == std::string_view::npos) {
            end = rest.size();
        }
        const std::string_view token = rest.substr(0, end);

        std::uint64_t value = 0;
        const char* begin = token.data();
        const char* last = token.data() + token.size();
        const auto result = std::from_chars(begin, last, value);
        if (result.ec != std::errc{} || result.ptr != last) {
            return std::nullopt;  // non-numeric, signed, or out of range
        }
        return value;
    }
    return std::nullopt;  // key not found
}

}  // namespace

namespace detail {

MemoryTelemetryService* makeWithSources(MemorySources sources, QObject* parent) {
    return new MemoryTelemetryService(std::move(sources), parent);
}

void pollOnceForTest(MemoryTelemetryService& service) { service.poll(); }

}  // namespace detail

MemoryTelemetryService::MemoryTelemetryService(QObject* parent)
    : MemoryTelemetryService(detail::MemorySources{readProcMeminfo, nullptr},
                             parent) {}

MemoryTelemetryService::MemoryTelemetryService(detail::MemorySources sources,
                                               QObject* parent)
    : ITelemetryProvider(parent), sources_(std::move(sources)),
      timer_(new QTimer(this)),
      current_(MetricSample::unavailable(MetricId::MemoryUtilization, 0)) {
    clock_.start();
    timer_->setInterval(kPollIntervalMs);
    connect(timer_, &QTimer::timeout, this, &MemoryTelemetryService::poll);
}

MemoryTelemetryService::~MemoryTelemetryService() { timer_->stop(); }

void MemoryTelemetryService::start() {
    if (timer_->isActive()) {
        return;
    }
    // No baseline to clear: memory utilization is an instantaneous ratio, so a
    // stop/start gap does not invalidate anything.
    timer_->start();
}

void MemoryTelemetryService::stop() {
    if (!timer_->isActive()) {
        return;
    }
    timer_->stop();
}

QList<models::MetricSample> MemoryTelemetryService::currentSamples() const {
    // The utilization sample is always present (Unavailable before the first
    // poll). The byte samples are included once produced, so the composition
    // root can prime a Memory instrument's full secondary line from
    // currentSamples() before the first live tick.
    QList<models::MetricSample> out{current_};
    if (lastUsedBytes_.has_value()) {
        out.append(*lastUsedBytes_);
    }
    if (lastTotalBytes_.has_value()) {
        out.append(*lastTotalBytes_);
    }
    return out;
}

void MemoryTelemetryService::emitSample(const MetricSample& sample) {
    const MetricState previous = current_.state();
    current_ = sample;
    if (sample.state() != previous) {
        // State-transition logging only: a persistently unreadable source
        // produces one entry per transition, not one per poll.
        qCInfo(lcMemoryTelemetry) << "memory telemetry state ->"
                                  << static_cast<int>(sample.state());
    }
    emit readingChanged(current_);
}

models::MetricSample MemoryTelemetryService::makeFailureSample(
    const char* reason) {
    const MonotonicTimestamp t =
        sources_.now ? sources_.now() : clock_.elapsed();

    if (lastValidValue_.has_value()) {
        if (const auto stale = MetricSample::tryStale(
                MetricId::MemoryUtilization, *lastValidValue_,
                MetricUnit::Percent, t)) {
            qCDebug(lcMemoryTelemetry) << "memory telemetry failure:" << reason;
            return *stale;
        }
    }
    qCDebug(lcMemoryTelemetry) << "memory telemetry failure:" << reason;
    return MetricSample::unavailable(MetricId::MemoryUtilization, t);
}

void MemoryTelemetryService::poll() {
    const MonotonicTimestamp t =
        sources_.now ? sources_.now() : clock_.elapsed();

    const std::optional<std::string> content =
        sources_.readMeminfo ? sources_.readMeminfo() : std::nullopt;
    if (!content.has_value()) {
        emitSample(makeFailureSample("read failed"));
        return;
    }

    const std::optional<std::uint64_t> total =
        valueForKey(*content, kMemTotalKey);
    if (!total.has_value()) {
        emitSample(makeFailureSample("MemTotal missing or malformed"));
        return;
    }

    // MemAvailable is required. No fallback formula is attempted: a substitute
    // calculation would report a different quantity under the same metric name.
    const std::optional<std::uint64_t> available =
        valueForKey(*content, kMemAvailableKey);
    if (!available.has_value()) {
        emitSample(makeFailureSample("MemAvailable missing or malformed"));
        return;
    }

    if (*total == 0) {
        emitSample(makeFailureSample("MemTotal is zero"));
        return;
    }
    if (*available > *total) {
        emitSample(makeFailureSample("MemAvailable exceeds MemTotal"));
        return;
    }

    const std::uint64_t used = *total - *available;
    double percent =
        100.0 * static_cast<double>(used) / static_cast<double>(*total);
    // Boundary-only clamp: absorbs negligible floating-point error at the
    // extremes. Invalid input has already been rejected above; this never masks
    // bad data.
    if (percent < 0.0) {
        percent = 0.0;
    } else if (percent > 100.0) {
        percent = 100.0;
    }

    const auto fresh = MetricSample::tryFresh(MetricId::MemoryUtilization,
                                              percent, MetricUnit::Percent, t);
    if (!fresh.has_value()) {
        // Unreachable given the guards above, but handled without fabricating a
        // value: treated as a calculation failure, not silently converted.
        emitSample(makeFailureSample("calculation produced no sample"));
        return;
    }

    lastValidValue_ = percent;

    // Additive: alongside the utilization percentage, emit the raw used/total
    // byte figures so a subsystem instrument can show "used / total GB" without
    // recomputing from /proc/meminfo. These are independent samples joined by
    // the consuming adapter, mirroring how GPU utilization and temperature are
    // separate samples. /proc/meminfo reports kB; convert to bytes (x1024).
    // Available memory is intentionally NOT emitted: the adapter derives it
    // cleanly as total - used from these two trustworthy values. The percentage
    // emission below is unchanged, so existing consumers (and the card
    // dashboard) see identical MemoryUtilization behavior.
    constexpr std::uint64_t kKbToBytes = 1024;
    const std::uint64_t usedBytes = used * kKbToBytes;
    const std::uint64_t totalBytes = *total * kKbToBytes;
    if (const auto usedSample = MetricSample::tryFresh(
            MetricId::MemoryUsedBytes, static_cast<double>(usedBytes),
            MetricUnit::Bytes, t)) {
        lastUsedBytes_ = usedSample;
        emit readingChanged(*usedSample);
    }
    if (const auto totalSample = MetricSample::tryFresh(
            MetricId::MemoryTotalBytes, static_cast<double>(totalBytes),
            MetricUnit::Bytes, t)) {
        lastTotalBytes_ = totalSample;
        emit readingChanged(*totalSample);
    }

    emitSample(*fresh);
}

}  // namespace darkspark::services
