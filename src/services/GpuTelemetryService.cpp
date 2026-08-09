// SPDX-License-Identifier: GPL-3.0-or-later
#include "services/GpuTelemetryService.hpp"

#include <QDir>
#include <QElapsedTimer>
#include <QLoggingCategory>

#include <charconv>
#include <cmath>
#include <fstream>
#include <sstream>
#include <string>

#include "models/MetricSample.hpp"
#include "services/GpuDeviceSelector.hpp"

namespace darkspark::services {

using models::MetricId;
using models::MetricSample;
using models::MetricUnit;
using models::MonotonicTimestamp;
// GpuDeviceSelection comes from services::selectGpuDevice().

namespace {

Q_LOGGING_CATEGORY(lcGpuTelemetry, "darkspark.telemetry.gpu")

constexpr int kPollIntervalMs = 1000;

/// Production reader: reads gpu_busy_percent from the resolved path. The path is
/// resolved once and captured; if it was empty (no device), every read fails.
std::function<std::optional<std::string>()> makeBusyReader() {
    // Use the shared GPU selection so utilization binds to the same physical
    // GPU as temperature (discrete preferred). Falls back to an empty path -- and
    // thus Unavailable readings -- when no AMD GPU is present.
    const std::optional<GpuDeviceSelection> sel = selectGpuDevice();
    const QString path = sel ? sel->busyPercentPath : QString();
    return [path]() -> std::optional<std::string> {
        if (path.isEmpty()) {
            return std::nullopt;
        }
        std::ifstream file(path.toStdString());
        if (!file.is_open()) {
            return std::nullopt;
        }
        std::stringstream ss;
        ss << file.rdbuf();
        if (file.bad()) {
            return std::nullopt;
        }
        return ss.str();
    };
}

/// Parse a gpu_busy_percent payload ("73\n") into a percentage in [0, 100].
/// Returns nullopt on empty or malformed content. Out-of-range integers are
/// rejected here; the adapter also clamps, but a value like 250 signals a
/// malformed read and is treated as no reading rather than a clamped 100.
std::optional<double> parseBusyPercent(const std::string& raw) {
    // Trim leading/trailing whitespace.
    std::size_t begin = raw.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return std::nullopt;
    }
    std::size_t end = raw.find_last_not_of(" \t\r\n");
    const std::string_view trimmed(raw.data() + begin, end - begin + 1);

    int value = 0;
    const char* first = trimmed.data();
    const char* last = trimmed.data() + trimmed.size();
    const auto [ptr, ec] = std::from_chars(first, last, value);
    if (ec != std::errc{} || ptr != last) {
        return std::nullopt;  // non-integer or trailing garbage
    }
    if (value < 0 || value > 100) {
        return std::nullopt;  // out of the driver's documented [0,100] range
    }
    return static_cast<double>(value);
}

}  // namespace

namespace detail {

GpuTelemetryService* makeWithSources(GpuUtilizationSources sources,
                                     QObject* parent) {
    return new GpuTelemetryService(std::move(sources), parent);
}

void pollOnceForTest(GpuTelemetryService& service) { service.poll(); }

}  // namespace detail

GpuTelemetryService::GpuTelemetryService(QObject* parent)
    : GpuTelemetryService(
          detail::GpuUtilizationSources{makeBusyReader(), nullptr}, parent) {}

GpuTelemetryService::GpuTelemetryService(detail::GpuUtilizationSources sources,
                                         QObject* parent)
    : ITelemetryProvider(parent), sources_(std::move(sources)),
      timer_(new QTimer(this)),
      current_(MetricSample::unavailable(MetricId::GpuTotalUtilization, 0)) {
    timer_->setInterval(kPollIntervalMs);
    connect(timer_, &QTimer::timeout, this, &GpuTelemetryService::poll);
}

GpuTelemetryService::~GpuTelemetryService() = default;

void GpuTelemetryService::start() {
    if (timer_->isActive()) {
        return;
    }
    // Diagnostic: confirm the service actually starts and whether it has a
    // reader bound (an empty selected path yields a null read -> Unavailable).
    qCInfo(lcGpuTelemetry)
        << "gpu utilization service start: poll=" << kPollIntervalMs
        << "ms readerBound=" << static_cast<bool>(sources_.readBusyPercent);
    timer_->start();
}

void GpuTelemetryService::stop() { timer_->stop(); }

QList<models::MetricSample> GpuTelemetryService::currentSamples() const {
    return {current_};
}

models::MetricSample GpuTelemetryService::computeSample() {
    static QElapsedTimer clock;
    if (!clock.isValid()) {
        clock.start();
    }
    const MonotonicTimestamp t =
        sources_.now ? sources_.now() : clock.elapsed();

    const std::optional<std::string> raw =
        sources_.readBusyPercent ? sources_.readBusyPercent() : std::nullopt;

    std::optional<double> value;
    if (raw.has_value()) {
        value = parseBusyPercent(*raw);
    }

    if (value.has_value()) {
        lastValidValue_ = value;
        const auto fresh = MetricSample::tryFresh(
            MetricId::GpuTotalUtilization, *value, MetricUnit::Percent, t);
        // tryFresh only fails for NaN/inf, which parseBusyPercent cannot yield.
        return fresh ? *fresh
                     : MetricSample::unavailable(MetricId::GpuTotalUtilization, t);
    }

    // No fresh value: retain the last valid one as Stale if we have it,
    // otherwise report Unavailable. Never fabricate.
    if (lastValidValue_.has_value()) {
        const auto stale = MetricSample::tryStale(MetricId::GpuTotalUtilization,
                                                  *lastValidValue_,
                                                  MetricUnit::Percent, t);
        if (stale) {
            return *stale;
        }
    }
    return MetricSample::unavailable(MetricId::GpuTotalUtilization, t);
}

void GpuTelemetryService::poll() {
    const MetricSample previous = current_;
    current_ = computeSample();
    if (current_.state() != previous.state()) {
        qCInfo(lcGpuTelemetry)
            << "gpu telemetry state ->" << static_cast<int>(current_.state());
    }
    // Diagnostic (throttled): log the value only when it moves by >=1 percentage
    // point, so a steady load doesn't flood the console every second.
    if (current_.value().has_value()) {
        const double v = current_.value().value();
        if (!lastLoggedValue_.has_value()
            || std::abs(v - *lastLoggedValue_) >= 1.0) {
            qCDebug(lcGpuTelemetry) << "gpu utilization =" << v << "%";
            lastLoggedValue_ = v;
        }
    }
    emit readingChanged(current_);
}

}  // namespace darkspark::services
