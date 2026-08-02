// SPDX-License-Identifier: GPL-3.0-or-later
#include "services/CpuThermalService.hpp"

#include <optional>
#include <utility>

#include <QByteArray>
#include <QFile>
#include <QLoggingCategory>
#include <QString>
#include <QStringList>
#include <QTimer>

namespace darkspark::services {

using models::MetricId;
using models::MetricSample;
using models::MetricState;
using models::MetricUnit;
using models::MonotonicTimestamp;

namespace {

Q_LOGGING_CATEGORY(lcCpuThermal, "darkspark.telemetry.cpu.thermal")

/// Fixed polling cadence. Deliberately a private constant.
constexpr int kPollIntervalMs = 1000;

/// Millidegrees Celsius per degree.
constexpr double kMilliPerDegree = 1000.0;

/// Production sensor discovery over the real hwmon hierarchy.
[[nodiscard]] QList<DiscoveredSensor> discoverReal() {
    return HwmonDiscovery(HwmonDiscovery::defaultRoot()).discover();
}

/// Production input read: interpret the file's content as millidegrees Celsius
/// and return degrees Celsius. Returns nullopt if unreadable or malformed.
[[nodiscard]] std::optional<double> readInputReal(const QString& inputPath) {
    QFile file(inputPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return std::nullopt;
    }
    const QByteArray raw = file.readAll();
    bool ok = false;
    const long long milli = QString::fromUtf8(raw).trimmed().toLongLong(&ok);
    if (!ok) {
        return std::nullopt;
    }
    return static_cast<double>(milli) / kMilliPerDegree;
}

/// "package" -> "Package", "ccd1" -> "CCD1" for readable logging.
[[nodiscard]] QString logLabelForKey(const std::string& key) {
    const QString k = QString::fromStdString(key);
    if (k == QStringLiteral("package")) {
        return QStringLiteral("Package");
    }
    if (k.startsWith(QStringLiteral("ccd"))) {
        return QStringLiteral("CCD") + k.mid(3);
    }
    return k;
}

}  // namespace

namespace detail {

CpuThermalService* makeWithSources(ThermalSources sources, QObject* parent) {
    return new CpuThermalService(std::move(sources), parent);
}

void pollOnceForTest(CpuThermalService& service) { service.poll(); }

}  // namespace detail

CpuThermalService::CpuThermalService(QObject* parent)
    : CpuThermalService(
          detail::ThermalSources{discoverReal, readInputReal, nullptr},
          parent) {}

CpuThermalService::CpuThermalService(detail::ThermalSources sources,
                                     QObject* parent)
    : ITelemetryProvider(parent), sources_(std::move(sources)),
      timer_(new QTimer(this)) {
    clock_.start();
    timer_->setInterval(kPollIntervalMs);
    connect(timer_, &QTimer::timeout, this, &CpuThermalService::poll);
}

CpuThermalService::~CpuThermalService() { timer_->stop(); }

void CpuThermalService::ensureDiscovered() {
    if (discovered_) {
        return;
    }
    discovered_ = true;

    const QList<DiscoveredSensor> found =
        sources_.discover ? sources_.discover() : QList<DiscoveredSensor>{};

    for (const DiscoveredSensor& sensor : found) {
        // Each discovered sensor starts Unavailable until its first read.
        tracked_.append(Tracked{
            sensor,
            MetricSample::unavailable(
                MetricId::CpuTemperature, 0, sensor.definition.identity.key),
            std::nullopt,
        });
    }

    if (tracked_.isEmpty()) {
        qCInfo(lcCpuThermal) << "[CPU Thermal] no CPU temperature sensors found";
    } else {
        for (const Tracked& t : tracked_) {
            qCInfo(lcCpuThermal).noquote()
                << QStringLiteral("[Sensor Discovery] CPU %1 temperature found")
                       .arg(logLabelForKey(t.sensor.definition.identity.key));
        }
    }
}

void CpuThermalService::start() {
    if (timer_->isActive()) {
        return;
    }
    ensureDiscovered();
    timer_->start();
}

void CpuThermalService::stop() {
    if (!timer_->isActive()) {
        return;
    }
    timer_->stop();
}

QList<models::MetricSample> CpuThermalService::currentSamples() const {
    QList<MetricSample> out;
    out.reserve(tracked_.size());
    for (const Tracked& t : tracked_) {
        out.append(t.current);
    }
    return out;
}

void CpuThermalService::emitSample(Tracked& tracked,
                                   const MetricSample& sample) {
    const MetricState previous = tracked.current.state();
    tracked.current = sample;
    if (sample.state() != previous) {
        // State-transition logging only, per sensor.
        qCInfo(lcCpuThermal).noquote()
            << QStringLiteral("[CPU Thermal] %1 state -> %2")
                   .arg(logLabelForKey(tracked.sensor.definition.identity.key))
                   .arg(static_cast<int>(sample.state()));
    }
    emit readingChanged(tracked.current);
}

void CpuThermalService::poll() {
    ensureDiscovered();

    const MonotonicTimestamp t =
        sources_.now ? sources_.now() : clock_.elapsed();

    QStringList readable;  // for the readable summary line
    for (Tracked& tracked : tracked_) {
        const std::string& key = tracked.sensor.definition.identity.key;

        const std::optional<double> value =
            sources_.readInput ? sources_.readInput(tracked.sensor.inputPath)
                               : std::nullopt;

        if (!value.has_value()) {
            // Read failed this cycle: Stale if we have a prior value, else
            // Unavailable. Debug level may carry the raw path.
            qCDebug(lcCpuThermal).noquote()
                << QStringLiteral("[CPU Thermal] read failed for %1 (%2)")
                       .arg(logLabelForKey(key), tracked.sensor.inputPath);
            if (tracked.lastValidValue.has_value()) {
                if (const auto stale = MetricSample::tryStale(
                        MetricId::CpuTemperature, *tracked.lastValidValue,
                        MetricUnit::Celsius, t, key)) {
                    emitSample(tracked, *stale);
                    continue;
                }
            }
            emitSample(tracked, MetricSample::unavailable(
                                    MetricId::CpuTemperature, t, key));
            continue;
        }

        const auto fresh = MetricSample::tryFresh(
            MetricId::CpuTemperature, *value, MetricUnit::Celsius, t, key);
        if (!fresh.has_value()) {
            // Non-finite value: treat as a failed read rather than fabricating.
            emitSample(tracked, MetricSample::unavailable(
                                    MetricId::CpuTemperature, t, key));
            continue;
        }
        tracked.lastValidValue = *value;
        emitSample(tracked, *fresh);
        readable << QStringLiteral("%1 %2\u00B0C")
                        .arg(logLabelForKey(key))
                        .arg(QString::number(*value, 'f', 0));
    }

    if (!readable.isEmpty()) {
        // Readable summary using logical names, never raw paths.
        qCInfo(lcCpuThermal).noquote()
            << QStringLiteral("[CPU Thermal] ") + readable.join(QStringLiteral(", "));
    }
}

}  // namespace darkspark::services
