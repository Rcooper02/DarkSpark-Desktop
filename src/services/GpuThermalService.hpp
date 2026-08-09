// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_SERVICES_GPUTHERMALSERVICE_HPP
#define DARKSPARK_SERVICES_GPUTHERMALSERVICE_HPP

#include <QString>
#include <QTimer>

#include <functional>
#include <optional>

#include "interfaces/ITelemetryProvider.hpp"
#include "models/MetricSample.hpp"

namespace darkspark::services {

// Forward-declared in the enclosing services namespace (NOT inside detail) so
// the detail helpers below refer to this class, services::GpuThermalService,
// rather than declaring a distinct detail::GpuThermalService.
class GpuThermalService;

namespace detail {

/// Injected collaborators for deterministic testing of GPU temperature.
///
/// GPU thermal discovery is GPU-owned and deliberately separate from the
/// CPU-only HwmonDiscovery (which is locked to k10temp with CPU label mapping).
///
/// `discoverInputPath` resolves the sysfs file to read for the primary GPU
/// temperature -- production: the amdgpu hwmon device's chosen tempN_input (see
/// selection rule in the .cpp); tests: a scripted path or nullopt. It is called
/// once per poll so a device appearing/disappearing is handled naturally.
/// `readMilliCelsius` reads the raw integer millidegrees-Celsius from a resolved
/// path (amdgpu reports temperatures in millidegrees). `now` returns a
/// monotonic timestamp. All have production defaults; tests script them so no
/// test touches real sysfs, the real clock, or a live GPU.
struct GpuThermalSources {
    std::function<std::optional<QString>()> discoverInputPath;
    std::function<std::optional<double>(const QString& inputPath)>
        readMilliCelsius;
    std::function<models::MonotonicTimestamp()> now;
};

[[nodiscard]] GpuThermalService* makeWithSources(GpuThermalSources sources,
                                                 QObject* parent);
void pollOnceForTest(GpuThermalService& service);

}  // namespace detail

/// GPU temperature telemetry for AMD (amdgpu), sourced from the GPU's hwmon
/// device.
///
/// Implements ITelemetryProvider for MetricId::GpuTemperature, publishing a
/// single primary temperature under the stable sensor key "gpu". The physical
/// sensor is selected once per poll by the discovery source; the published key
/// is stable regardless of which label the hardware exposed, so the adapter and
/// instrument never depend on hardware specifics.
///
/// Selection rule (production, see .cpp): among the amdgpu hwmon device's
/// tempN_label entries, prefer "junction" (the GPU hotspot) if present,
/// otherwise "edge". "mem" and any other labels are not used in this milestone.
///
/// A missing device, missing label, or malformed value yields Unavailable (or
/// Stale if a prior valid value exists), never a throw.
///
/// Ownership: a QObject owned by its Qt parent. Threading: GUI thread only.
class GpuThermalService : public interfaces::ITelemetryProvider {
    Q_OBJECT

public:
    static constexpr const char* kPrimaryKey = "gpu";

    explicit GpuThermalService(QObject* parent = nullptr);
    ~GpuThermalService() override;

    void start() override;
    void stop() override;

    [[nodiscard]] QList<models::MetricSample> currentSamples() const override;

private:
    friend GpuThermalService* detail::makeWithSources(detail::GpuThermalSources,
                                                      QObject*);
    friend void detail::pollOnceForTest(GpuThermalService&);

    GpuThermalService(detail::GpuThermalSources sources, QObject* parent);

    void poll();
    [[nodiscard]] models::MetricSample computeSample();

    detail::GpuThermalSources sources_;
    QTimer* timer_;
    std::optional<double> lastValidValue_;
    std::optional<double> lastLoggedValue_;  ///< diagnostic throttle (>=1 C)
    models::MetricSample current_;
};

}  // namespace darkspark::services

#endif  // DARKSPARK_SERVICES_GPUTHERMALSERVICE_HPP
