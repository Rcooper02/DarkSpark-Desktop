// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_SERVICES_COOLINGSENSORPROVIDER_HPP
#define DARKSPARK_SERVICES_COOLINGSENSORPROVIDER_HPP

#include <QString>
#include <QStringList>

#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace darkspark::services {

/// Provider-independent description of a sensor's identity and capabilities.
///
/// This belongs ENTIRELY to the provider layer: a provider (hwmon today;
/// liquidctl, lm-sensors, vendor APIs, or a future Windows/cloud provider later)
/// fills it in from its native representation. Consumers (CoolingTelemetryService)
/// only READ it -- for discovery logging, stable selection, and diagnostics --
/// and never modify it. Wrapping the fields in one struct keeps the provider
/// contract clean and makes future expansion painless (add a field here, no
/// signature churn).
///
/// Cooling V1 populates only what hwmon can supply (label, stableId, devicePath);
/// the remaining fields exist so future providers and future Health Engine /
/// diagnostics work can carry manufacturer, model, firmware, and capability
/// hints without a contract change. They are not consumed today.
struct SensorMetadata {
    QString manufacturer;      ///< e.g. "NZXT", "ASUS"; empty if unknown
    QString model;             ///< e.g. "Kraken X63"; empty if unknown
    QString label;             ///< human sensor label, e.g. "CPU FAN", "pump"
    QString stableId;          ///< stable identity, e.g. "nct6798:fan1" (NOT hwmonN)
    QString devicePath;        ///< transport path, e.g. a /sys or bus path
    QString firmware;          ///< firmware version if a provider exposes it
    QStringList capabilities;  ///< capability hints, e.g. "rpm", "pwm", "control"
};

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
