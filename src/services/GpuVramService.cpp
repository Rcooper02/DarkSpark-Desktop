// SPDX-License-Identifier: GPL-3.0-or-later
#include "services/GpuVramService.hpp"

#include <QElapsedTimer>
#include <QFile>
#include <QLoggingCategory>
#include <QTextStream>

#include <cstdint>
#include <optional>

#include "models/MetricSample.hpp"
#include "services/GpuDeviceSelector.hpp"

namespace darkspark::services {

using models::MetricId;
using models::MetricSample;
using models::MetricState;
using models::MetricUnit;
using models::MonotonicTimestamp;

namespace {

Q_LOGGING_CATEGORY(lcGpuVram, "darkspark.telemetry.gpu.vram")

constexpr int kPollIntervalMs = 1000;

/// Read a small sysfs text file fully, trimmed, and parse it as an unsigned
/// 64-bit integer (amdgpu reports VRAM figures in bytes). Returns nullopt on any
/// failure: missing file, empty content, or non-integer text.
std::optional<std::uint64_t> readSysfsU64(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return std::nullopt;
    }
    QTextStream in(&f);
    const QString s = in.readAll().trimmed();
    if (s.isEmpty()) {
        return std::nullopt;
    }
    bool ok = false;
    const qulonglong v = s.toULongLong(&ok);
    if (!ok) {
        return std::nullopt;
    }
    return static_cast<std::uint64_t>(v);
}

/// Production reader: resolve the selected GPU's device directory ONCE (shared
/// selector, same physical GPU as utilization/temperature), capture the two
/// VRAM paths, and read them each poll. Resolving once avoids re-running
/// selection on every poll. Returns nullopt when no GPU was selected, either
/// file is missing/malformed, or used > total (a nonsensical reading treated as
/// no reading rather than a clamped value).
std::function<std::optional<detail::GpuVramReading>()> makeVramReader() {
    const std::optional<GpuDeviceSelection> sel = selectGpuDevice();
    // mem_info_vram_* live in the same /device directory as gpu_busy_percent.
    QString usedPath;
    QString totalPath;
    if (sel && !sel->drmCardPath.isEmpty()) {
        const QString dev = sel->drmCardPath + QStringLiteral("/device/");
        usedPath = dev + QStringLiteral("mem_info_vram_used");
        totalPath = dev + QStringLiteral("mem_info_vram_total");
    }
    return [usedPath, totalPath]() -> std::optional<detail::GpuVramReading> {
        if (usedPath.isEmpty() || totalPath.isEmpty()) {
            return std::nullopt;
        }
        const std::optional<std::uint64_t> used = readSysfsU64(usedPath);
        const std::optional<std::uint64_t> total = readSysfsU64(totalPath);
        if (!used || !total) {
            return std::nullopt;
        }
        if (*total == 0 || *used > *total) {
            return std::nullopt;  // nonsensical reading: treat as no reading
        }
        return detail::GpuVramReading{*used, *total};
    };
}

}  // namespace

namespace detail {

GpuVramService* makeWithSources(GpuVramSources sources, QObject* parent) {
    return new GpuVramService(std::move(sources), parent);
}

void pollOnceForTest(GpuVramService& service) { service.poll(); }

}  // namespace detail

GpuVramService::GpuVramService(QObject* parent)
    : GpuVramService(detail::GpuVramSources{makeVramReader(), nullptr},
                     parent) {}

GpuVramService::GpuVramService(detail::GpuVramSources sources, QObject* parent)
    : ITelemetryProvider(parent), sources_(std::move(sources)),
      timer_(new QTimer(this)),
      currentUsed_(
          MetricSample::unavailable(MetricId::MemoryUsedBytes, 0, kVramKey)),
      currentTotal_(
          MetricSample::unavailable(MetricId::MemoryTotalBytes, 0, kVramKey)) {
    timer_->setInterval(kPollIntervalMs);
    connect(timer_, &QTimer::timeout, this, &GpuVramService::poll);
}

GpuVramService::~GpuVramService() = default;

void GpuVramService::start() {
    if (timer_->isActive()) {
        return;
    }
    // Diagnostic: confirm the service starts and whether a reader is bound (an
    // unselected GPU yields a null read -> Unavailable, degrades gracefully).
    qCInfo(lcGpuVram) << "gpu vram service start: poll=" << kPollIntervalMs
                      << "ms readerBound=" << static_cast<bool>(sources_.readVram);
    timer_->start();
}

void GpuVramService::stop() { timer_->stop(); }

QList<models::MetricSample> GpuVramService::currentSamples() const {
    return {currentUsed_, currentTotal_};
}

void GpuVramService::poll() {
    static QElapsedTimer clock;
    if (!clock.isValid()) {
        clock.start();
    }
    const MonotonicTimestamp t = sources_.now ? sources_.now() : clock.elapsed();

    const std::optional<detail::GpuVramReading> reading =
        sources_.readVram ? sources_.readVram() : std::nullopt;

    const MetricState prevUsedState = currentUsed_.state();

    if (reading.has_value()) {
        lastValid_ = reading;
        const auto usedS = MetricSample::tryFresh(
            MetricId::MemoryUsedBytes,
            static_cast<double>(reading->usedBytes), MetricUnit::Bytes, t,
            kVramKey);
        const auto totalS = MetricSample::tryFresh(
            MetricId::MemoryTotalBytes,
            static_cast<double>(reading->totalBytes), MetricUnit::Bytes, t,
            kVramKey);
        currentUsed_ = usedS ? *usedS
                             : MetricSample::unavailable(
                                   MetricId::MemoryUsedBytes, t, kVramKey);
        currentTotal_ = totalS ? *totalS
                               : MetricSample::unavailable(
                                     MetricId::MemoryTotalBytes, t, kVramKey);
    } else if (lastValid_.has_value()) {
        // Retain the last good reading as Stale; never fabricate.
        const auto usedS = MetricSample::tryStale(
            MetricId::MemoryUsedBytes,
            static_cast<double>(lastValid_->usedBytes), MetricUnit::Bytes, t,
            kVramKey);
        const auto totalS = MetricSample::tryStale(
            MetricId::MemoryTotalBytes,
            static_cast<double>(lastValid_->totalBytes), MetricUnit::Bytes, t,
            kVramKey);
        currentUsed_ = usedS ? *usedS
                             : MetricSample::unavailable(
                                   MetricId::MemoryUsedBytes, t, kVramKey);
        currentTotal_ = totalS ? *totalS
                               : MetricSample::unavailable(
                                     MetricId::MemoryTotalBytes, t, kVramKey);
    } else {
        currentUsed_ =
            MetricSample::unavailable(MetricId::MemoryUsedBytes, t, kVramKey);
        currentTotal_ =
            MetricSample::unavailable(MetricId::MemoryTotalBytes, t, kVramKey);
    }

    if (currentUsed_.state() != prevUsedState) {
        qCInfo(lcGpuVram) << "gpu vram state ->"
                          << static_cast<int>(currentUsed_.state());
    }

    emit readingChanged(currentUsed_);
    emit readingChanged(currentTotal_);
}

}  // namespace darkspark::services
