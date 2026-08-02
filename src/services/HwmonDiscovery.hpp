// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_SERVICES_HWMONDISCOVERY_HPP
#define DARKSPARK_SERVICES_HWMONDISCOVERY_HPP

#include <QList>
#include <QString>

#include "models/SensorDefinition.hpp"

namespace darkspark::services {

/// A sensor discovered on this machine, bound to the file that provides its
/// reading.
///
/// This is the binding layer of the frozen telemetry architecture:
///
///     SensorDefinition   (transport-independent -- what the sensor is)
///         -> Discovery / binding  (THIS type -- it is here, and here is where)
///         -> Provider             (reads inputPath)
///         -> MetricSample         (the runtime reading + data quality)
///
/// The transport-independent description lives in `definition`; the
/// Linux-specific location lives in `inputPath`. The path is deliberately kept
/// OUT of SensorDefinition so the definition stays platform-agnostic. A future
/// provider reads `inputPath` to obtain millidegrees Celsius.
struct DiscoveredSensor {
    models::SensorDefinition definition{};

    /// Absolute path to the hwmon temp*_input file that provides this sensor's
    /// value (millidegrees Celsius). Transport-specific; never surfaced through
    /// SensorDefinition.
    QString inputPath{};

    friend bool operator==(const DiscoveredSensor&,
                           const DiscoveredSensor&) = default;
};

/// Standalone, Linux-specific discovery of hwmon temperature sensors.
///
/// This is a discovery UTILITY, not a telemetry provider and not a generic
/// framework. It does one job: given a root directory (production:
/// "/sys/class/hwmon"), walk it and return the CPU temperature sensors it can
/// bind, each paired with the file a provider will read.
///
/// Path independence: hwmon device numbering (hwmon0, hwmon1, ...) is unstable
/// across boots and hardware changes, so discovery never assumes a fixed
/// hwmonN. It enumerates whatever hwmon* directories exist, identifies devices
/// by their `name` file, and re-resolves on every call rather than caching
/// paths. Callers that keep results across time must tolerate a path that has
/// since disappeared (a later read simply fails and the provider reports
/// Unavailable).
///
/// Scope for this batch: only `k10temp` devices (AMD desktop/Zen CPU
/// temperatures) are selected. Other hwmon devices are ignored. Threading: no
/// Qt object state; safe to call from the GUI thread.
class HwmonDiscovery {
public:
    /// Construct a discovery bound to a filesystem root. Tests pass a synthetic
    /// temporary directory; production passes "/sys/class/hwmon".
    explicit HwmonDiscovery(QString root);

    /// The default production root: "/sys/class/hwmon".
    [[nodiscard]] static QString defaultRoot();

    /// Discover CPU temperature sensors under the root, freshly resolved.
    ///
    /// Returns one DiscoveredSensor per successfully bound label (a label with a
    /// readable, well-formed paired input). Devices that are not k10temp, labels
    /// without a paired input, and malformed entries are skipped. The result is
    /// ordered deterministically by sensor key so callers and tests see a
    /// stable sequence regardless of directory iteration order.
    [[nodiscard]] QList<DiscoveredSensor> discover() const;

private:
    QString root_;
};

}  // namespace darkspark::services

#endif  // DARKSPARK_SERVICES_HWMONDISCOVERY_HPP
