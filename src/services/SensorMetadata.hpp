// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_SERVICES_SENSORMETADATA_HPP
#define DARKSPARK_SERVICES_SENSORMETADATA_HPP

#include <QString>
#include <QStringList>

namespace darkspark::services {

/// Provider-independent description of a sensor's identity and capabilities.
///
/// Part of the shared telemetry framework, not any one subsystem: every
/// provider stack (Cooling, Storage today; Network, Battery, Power later) uses
/// this same struct, so it lives in a shared header rather than being duplicated
/// per subsystem. It belongs ENTIRELY to the provider layer: a provider fills it
/// in from its native representation, and consumers (the telemetry services)
/// only READ it -- for discovery logging, stable selection, and diagnostics --
/// and never modify it. Wrapping the fields in one struct keeps every provider
/// contract clean and makes future expansion painless (add a field here, no
/// signature churn across subsystems).
///
/// Subsystems populate only what their transport can supply (e.g. hwmon gives
/// label/stableId/devicePath); the remaining fields exist so future providers
/// and future Health Engine / diagnostics work can carry manufacturer, model,
/// firmware, and capability hints without a contract change.
struct SensorMetadata {
    QString manufacturer;      ///< e.g. "NZXT", "ASUS", "Samsung"; empty if unknown
    QString model;             ///< e.g. "Kraken X63", "990 PRO"; empty if unknown
    QString label;             ///< human sensor label, e.g. "CPU FAN", "nvme0"
    QString stableId;          ///< stable identity, e.g. "nct6798:fan1" (NOT an index)
    QString devicePath;        ///< transport path, e.g. a /sys, /dev, or mount path
    QString firmware;          ///< firmware version if a provider exposes it
    QStringList capabilities;  ///< capability hints, e.g. "rpm", "temp", "throughput"
};

}  // namespace darkspark::services

#endif  // DARKSPARK_SERVICES_SENSORMETADATA_HPP
