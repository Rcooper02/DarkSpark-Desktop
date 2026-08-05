// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_SERVICES_HWMONCOOLINGPROVIDER_HPP
#define DARKSPARK_SERVICES_HWMONCOOLINGPROVIDER_HPP

#include <QString>

#include "services/CoolingSensorProvider.hpp"

namespace darkspark::services {

/// The first CoolingSensorProvider: reads fan/pump RPM and coolant temperature
/// from Linux hwmon (/sys/class/hwmon).
///
/// Discovery discipline (mirrors HwmonDiscovery): identify devices by their
/// `name` file, re-resolve every call, NEVER cache or persist hwmonN indices.
/// Stable identity is the hwmon name plus the sensor's own base (e.g.
/// "nct6798:fan1"), carried in SensorMetadata.stableId. A fan on the selected
/// discrete GPU's hwmon (via GpuDeviceSelector) is tagged GpuFanRpm so it binds
/// to the same physical GPU the rest of the app uses.
///
/// Role hints from label/name:
///   - label contains "pump"            -> PumpRpm
///   - on the selected GPU hwmon        -> GpuFanRpm
///   - label/name suggests CPU/super-IO -> CpuFanRpm
///   - otherwise                        -> CaseFanRpm
///   - temp label contains coolant/water-> CoolantTemp
///
/// The root is injectable so tests drive a synthetic /sys tree; production uses
/// "/sys/class/hwmon".
class HwmonCoolingProvider : public CoolingSensorProvider {
public:
    explicit HwmonCoolingProvider(QString root = defaultRoot());

    [[nodiscard]] static QString defaultRoot();

    [[nodiscard]] QString providerName() const override;
    [[nodiscard]] std::vector<NormalizedCoolingSensor> discover() const override;

    /// The GPU hwmon path used to tag GPU fans. Injectable for tests; production
    /// resolves it from GpuDeviceSelector. Empty when there is no GPU hwmon.
    void setGpuHwmonPathForTest(QString path) { gpuHwmonPath_ = std::move(path); }

private:
    QString root_;
    QString gpuHwmonPath_;
    bool gpuResolved_ = false;

    [[nodiscard]] QString gpuHwmonPath() const;
};

}  // namespace darkspark::services

#endif  // DARKSPARK_SERVICES_HWMONCOOLINGPROVIDER_HPP
