// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_MODELS_SENSORDEFINITION_HPP
#define DARKSPARK_MODELS_SENSORDEFINITION_HPP
#include <string>
#include "models/MetricSample.hpp"
namespace darkspark::models {

/// Static, transport-independent description of a KIND of sensor.
///
/// A SensorDefinition is the catalog entry for a sensor: what it is, what it is
/// called, what unit it reports, and which metric category it belongs to. It
/// exists whether or not the hardware is present on this machine, and it is
/// identical regardless of how the value is obtained.
///
/// Transport independence is deliberate and load-bearing. A SensorDefinition
/// must NEVER know whether the reading arrives via hwmon, liquidctl, smartctl,
/// NVML, ADLX, WMI, IPMI, Home Assistant, Proxmox, SNMP, MQTT, USB HID, or any
/// future local or remote provider. Discovery binds a definition to a
/// platform-specific source; a provider reads that source and emits
/// MetricSample values. Paths, device names, provider types, and transport
/// details live in discovery/providers, never here.
///
///     SensorDefinition            (this type -- what a sensor is)
///         -> Discovery / binding  (is it here on this platform, and where)
///         -> Provider             (read the bound source)
///         -> MetricSample         (the runtime reading + data quality)
///
/// Runtime values and data quality are NOT stored here; those belong to
/// MetricSample. Health thresholds are NOT stored here; those belong to the
/// future Health Engine.
///
/// Future subsystem grouping (architectural reservation only -- not implemented
/// in T7A): sensors will eventually belong to user-facing subsystems, giving a
/// hierarchy of SubsystemDefinition -> SensorDefinition -> Discovery ->
/// Provider -> MetricSample. Example subsystems and their sensors:
///   CPU:     utilization, package temperature, CCD temperatures, per-core load
///   GPU:     utilization, core temperature, hotspot, VRAM temperature, memory
///   Cooling: pump RPM, coolant temperature, fan RPMs
///   Storage: NVMe temperatures, health, capacity
///   System:  uptime, kernel health, services
/// No SubsystemDefinition type or subsystem field is added in T7A.
///
/// Capability flags (supports history / alerts / trend / AI analysis / user
/// thresholds / ...) are intentionally NOT added yet: T7A has no consumer that
/// reads them, and speculative flags are the same trap as speculative enum
/// values. They will be added when a real consumer distinguishes them.
struct SensorDefinition {
    /// Composite identity this definition describes (category + stable key).
    /// This is the join key that ties the definition to live samples and, in
    /// future, to profiles, history, health, and widgets.
    SensorKey identity{};

    /// Human-facing name used in diagnostics and, later, the UI
    /// (for example "CPU Package" or "CPU CCD1").
    std::string displayName{};

    /// Unit the sensor reports.
    MetricUnit unit{MetricUnit::Percent};

    [[nodiscard]] MetricId category() const { return identity.category; }
    [[nodiscard]] const std::string& key() const { return identity.key; }

    friend bool operator==(const SensorDefinition&,
                           const SensorDefinition&) = default;
};
}
#endif
