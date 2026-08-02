// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_SERVICES_CPUTHERMALSERVICE_HPP
#define DARKSPARK_SERVICES_CPUTHERMALSERVICE_HPP

#include <functional>
#include <optional>

#include <QElapsedTimer>
#include <QList>
#include <QObject>
#include <QString>

#include "interfaces/ITelemetryProvider.hpp"
#include "models/MetricSample.hpp"
#include "services/HwmonDiscovery.hpp"

class QTimer;

namespace darkspark::services {

class CpuThermalService;

namespace detail {

/// Injected collaborators for deterministic testing.
///
/// `discover` returns the CPU temperature sensors to sample (production: a real
/// HwmonDiscovery over /sys/class/hwmon; tests: a discovery over a synthetic
/// temporary tree). `readInput` reads the raw millidegrees-Celsius value from a
/// sensor's bound input path, returning nullopt if the value cannot be read or
/// is malformed. `now` returns a monotonic timestamp in milliseconds.
///
/// This is the same seam shape used by the other providers: no test depends on
/// the real /sys hierarchy, the real clock, or live CPU temperature.
struct ThermalSources {
    std::function<QList<DiscoveredSensor>()> discover;
    std::function<std::optional<double>(const QString& inputPath)> readInput;
    std::function<models::MonotonicTimestamp()> now;
};

/// Internal access helper: constructs a CpuThermalService with injected sources.
[[nodiscard]] CpuThermalService* makeWithSources(ThermalSources sources,
                                                 QObject* parent);

/// Internal access helper: performs exactly one sampling cycle synchronously.
/// Production code never calls this; the service polls itself on its timer.
void pollOnceForTest(CpuThermalService& service);

}  // namespace detail

/// Live CPU temperature telemetry, sourced from hwmon via HwmonDiscovery.
///
/// Implements ITelemetryProvider directly (it does not extend
/// CpuTelemetryService: utilization and temperature are different sources with
/// different failure modes and lifecycles). It emits one MetricSample per
/// discovered CPU temperature sensor -- typically a package temperature plus one
/// per CCD -- all under MetricId::CpuTemperature, distinguished by stable sensor
/// key ("package", "ccd1", ...).
///
/// Temperature is an instantaneous reading: there is no baseline, no delta, and
/// no re-baselining. Each poll reads the current value of each discovered
/// sensor. A sensor read that fails yields Stale (if a prior value exists for
/// that sensor) or Unavailable.
///
/// Discovery is performed once at start(): CPU temperature sensors do not
/// hot-plug. Values are re-read every poll, so a sensor whose file later
/// disappears transitions to Stale/Unavailable without the provider assuming a
/// fixed path is permanently valid.
///
/// Ownership: a QObject owned by its Qt parent. Threading: GUI thread only.
class CpuThermalService : public interfaces::ITelemetryProvider {
    Q_OBJECT

public:
    explicit CpuThermalService(QObject* parent = nullptr);
    ~CpuThermalService() override;

    /// Discover sensors (once) and begin sampling. Idempotent while running.
    void start() override;

    /// Stop sampling. Idempotent. The last emitted samples remain queryable.
    void stop() override;

    /// The most recent sample for each discovered sensor. Performs no I/O.
    /// Before the first start()/poll this is empty; once sensors are discovered
    /// each is represented, Unavailable until first successfully read.
    [[nodiscard]] QList<models::MetricSample> currentSamples() const override;

private:
    friend CpuThermalService* detail::makeWithSources(
        detail::ThermalSources sources, QObject* parent);
    friend void detail::pollOnceForTest(CpuThermalService& service);

    CpuThermalService(detail::ThermalSources sources, QObject* parent);

    /// One tracked sensor: its binding plus its last emitted sample and last
    /// valid value (for Stale retention).
    struct Tracked {
        DiscoveredSensor sensor;
        models::MetricSample current;
        std::optional<double> lastValidValue;
    };

    void poll();
    void ensureDiscovered();
    void emitSample(Tracked& tracked, const models::MetricSample& sample);

    detail::ThermalSources sources_;
    QTimer* timer_;
    QElapsedTimer clock_;

    QList<Tracked> tracked_;
    bool discovered_ = false;
};

}  // namespace darkspark::services

#endif  // DARKSPARK_SERVICES_CPUTHERMALSERVICE_HPP
