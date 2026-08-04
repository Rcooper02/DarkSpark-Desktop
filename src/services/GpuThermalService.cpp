// SPDX-License-Identifier: GPL-3.0-or-later
#include "services/GpuThermalService.hpp"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QTextStream>

#include <fstream>
#include <sstream>

#include "models/MetricSample.hpp"
#include "services/GpuDeviceSelector.hpp"

namespace darkspark::services {

using models::MetricId;
using models::MetricSample;
using models::MetricUnit;
using models::MonotonicTimestamp;

namespace {

Q_LOGGING_CATEGORY(lcGpuThermal, "darkspark.telemetry.gpu.thermal")

constexpr int kPollIntervalMs = 1000;

/// Read a small sysfs text file fully, trimmed. Returns nullopt on any failure.
std::optional<QString> readSysfsText(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return std::nullopt;
    }
    QTextStream in(&f);
    const QString s = in.readAll().trimmed();
    if (s.isEmpty()) {
        return std::nullopt;
    }
    return s;
}

/// Production discovery: resolve the amdgpu hwmon tempN_input path for the
/// primary GPU temperature.
///
/// Selection rule: scan tempN_label files; prefer the input whose label is
/// "junction" (the GPU hotspot AMD's driver exposes and the temperature that
/// matters for GPU health). If no junction label exists, fall back to the
/// input whose label is "edge" (the board-edge sensor present on essentially
/// all cards). "mem" and any other labels are ignored in this milestone. If a
/// device has temp inputs but no labels at all, temp1_input is used as a last
/// resort (some minimal setups omit labels; temp1 is conventionally the edge).
std::optional<QString> resolveAmdgpuTempInput() {
    // Use the shared GPU selection so temperature binds to the SAME physical GPU
    // as utilization. If the selection has no matching hwmon (temperature
    // sensor absent), return nullopt -- utilization still works independently.
    const std::optional<GpuDeviceSelection> sel = selectGpuDevice();
    if (!sel || !sel->hasHwmon) {
        return std::nullopt;
    }
    const QString dir = sel->hwmonPath;
    QDir d(dir);
    const QStringList labels =
        d.entryList(QStringList{QStringLiteral("temp*_label")}, QDir::Files,
                    QDir::Name);

    QString edgeInput;
    QString firstInput;
    for (const QString& labelFile : labels) {
        const std::optional<QString> label =
            readSysfsText(d.absoluteFilePath(labelFile));
        if (!label) {
            continue;
        }
        // temp1_label -> temp1_input
        QString inputFile = labelFile;
        inputFile.replace(QStringLiteral("_label"), QStringLiteral("_input"));
        const QString inputPath = d.absoluteFilePath(inputFile);
        if (!QFile::exists(inputPath)) {
            continue;
        }
        if (firstInput.isEmpty()) {
            firstInput = inputPath;
        }
        if (label->compare(QStringLiteral("junction"), Qt::CaseInsensitive) == 0) {
            return inputPath;  // best choice, return immediately
        }
        if (label->compare(QStringLiteral("edge"), Qt::CaseInsensitive) == 0) {
            edgeInput = inputPath;
        }
    }
    if (!edgeInput.isEmpty()) {
        return edgeInput;
    }
    // No junction and no edge label: last-resort temp1_input if present.
    const QString temp1 = d.absoluteFilePath(QStringLiteral("temp1_input"));
    if (QFile::exists(temp1)) {
        return temp1;
    }
    if (!firstInput.isEmpty()) {
        return firstInput;
    }
    return std::nullopt;
}

/// Production read: amdgpu reports temperature in millidegrees Celsius.
std::optional<double> readMilliCelsiusFile(const QString& inputPath) {
    const std::optional<QString> raw = readSysfsText(inputPath);
    if (!raw) {
        return std::nullopt;
    }
    bool ok = false;
    const long long milli = raw->toLongLong(&ok);
    if (!ok) {
        return std::nullopt;
    }
    return static_cast<double>(milli) / 1000.0;
}

}  // namespace

namespace detail {

GpuThermalService* makeWithSources(GpuThermalSources sources, QObject* parent) {
    return new GpuThermalService(std::move(sources), parent);
}

void pollOnceForTest(GpuThermalService& service) { service.poll(); }

}  // namespace detail

GpuThermalService::GpuThermalService(QObject* parent)
    : GpuThermalService(
          detail::GpuThermalSources{resolveAmdgpuTempInput,
                                    readMilliCelsiusFile, nullptr},
          parent) {}

GpuThermalService::GpuThermalService(detail::GpuThermalSources sources,
                                     QObject* parent)
    : ITelemetryProvider(parent), sources_(std::move(sources)),
      timer_(new QTimer(this)),
      current_(MetricSample::unavailable(MetricId::GpuTemperature, 0,
                                         kPrimaryKey)) {
    timer_->setInterval(kPollIntervalMs);
    connect(timer_, &QTimer::timeout, this, &GpuThermalService::poll);
}

GpuThermalService::~GpuThermalService() = default;

void GpuThermalService::start() {
    if (timer_->isActive()) {
        return;
    }
    timer_->start();
}

void GpuThermalService::stop() { timer_->stop(); }

QList<models::MetricSample> GpuThermalService::currentSamples() const {
    return {current_};
}

models::MetricSample GpuThermalService::computeSample() {
    static QElapsedTimer clock;
    if (!clock.isValid()) {
        clock.start();
    }
    const MonotonicTimestamp t =
        sources_.now ? sources_.now() : clock.elapsed();

    std::optional<QString> path =
        sources_.discoverInputPath ? sources_.discoverInputPath() : std::nullopt;

    std::optional<double> value;
    if (path.has_value() && sources_.readMilliCelsius) {
        value = sources_.readMilliCelsius(*path);
    }

    if (value.has_value()) {
        lastValidValue_ = value;
        const auto fresh = MetricSample::tryFresh(
            MetricId::GpuTemperature, *value, MetricUnit::Celsius, t, kPrimaryKey);
        return fresh ? *fresh
                     : MetricSample::unavailable(MetricId::GpuTemperature, t,
                                                 kPrimaryKey);
    }

    if (lastValidValue_.has_value()) {
        const auto stale = MetricSample::tryStale(MetricId::GpuTemperature,
                                                  *lastValidValue_,
                                                  MetricUnit::Celsius, t,
                                                  kPrimaryKey);
        if (stale) {
            return *stale;
        }
    }
    return MetricSample::unavailable(MetricId::GpuTemperature, t, kPrimaryKey);
}

void GpuThermalService::poll() {
    const MetricSample previous = current_;
    current_ = computeSample();
    if (current_.state() != previous.state()) {
        qCInfo(lcGpuThermal)
            << "gpu thermal state ->" << static_cast<int>(current_.state());
    }
    emit readingChanged(current_);
}

}  // namespace darkspark::services
