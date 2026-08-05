// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_SERVICES_COOLINGSENSORPROVIDER_HPP
#define DARKSPARK_SERVICES_COOLINGSENSORPROVIDER_HPP

#include <QString>
#include <QStringList>

#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "services/SensorMetadata.hpp"

namespace darkspark::services {

/// The role a discovered cooling sensor plays. A provider assigns a role HINT;
/// the service's selection turns hints into the logical primary/secondary.
enum class CoolingSensorRole {
    PumpRpm,      ///< a pump's tachometer
    CpuFanRpm,    ///< a CPU/package fan
    GpuFanRpm,    ///< a fan on the selected discrete GPU
    CaseFanRpm,   ///< an identifiable case fan
    CoolantTemp   ///< coolant/liquid temperature
};

/// The unit a normalized sensor reports in. Kept provider-agnostic so the
/// service can attach the right MetricUnit without knowing the source.
enum class CoolingSensorUnit {
    Rpm,
    Celsius
};

/// A provider-independent sensor the service can read, with its identity/metadata
/// and a read function. Providers translate their native sensors into this
/// common shape, so the service consumes normalized objects and never cares
/// where they originated.
struct NormalizedCoolingSensor {
    CoolingSensorRole role = CoolingSensorRole::CaseFanRpm;
    CoolingSensorUnit unit = CoolingSensorUnit::Rpm;
    SensorMetadata metadata;
    /// Read the current value in the sensor's unit (RPM, or degrees Celsius).
    /// Returns nullopt on a missing/malformed reading -- the service then reports
    /// Unavailable/Stale and never fabricates a value. Re-resolved per read so a
    /// path that has disappeared simply fails honestly.
    std::function<std::optional<double>()> read;
};

/// Interface every cooling sensor source implements. This is the permanent
/// DarkSpark discovery pattern: subsystems consume providers, and new sources
/// (liquidctl, lm-sensors, vendor APIs, Windows, ...) are added as new providers
/// WITHOUT changing the service, adapter, instrument, telemetry contract, or
/// renderer. hwmon is simply the first provider.
class CoolingSensorProvider {
public:
    virtual ~CoolingSensorProvider() = default;

    /// A short provider identity for discovery logging, e.g. "hwmon".
    [[nodiscard]] virtual QString providerName() const = 0;

    /// Enumerate the cooling sensors this provider can currently see, each
    /// normalized. Freshly resolved on each call; never caches unstable indices.
    [[nodiscard]] virtual std::vector<NormalizedCoolingSensor> discover()
        const = 0;
};

using CoolingSensorProviderPtr = std::shared_ptr<CoolingSensorProvider>;

}  // namespace darkspark::services

#endif  // DARKSPARK_SERVICES_COOLINGSENSORPROVIDER_HPP
