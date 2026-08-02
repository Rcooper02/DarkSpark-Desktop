// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_MODELS_METRICSAMPLE_HPP
#define DARKSPARK_MODELS_METRICSAMPLE_HPP
#include <cstdint>
#include <optional>
#include <string>
namespace darkspark::models {

/// Broad category of a metric. Kept small on purpose: additional sensors within
/// a category (for example multiple CPU CCD temperatures) are distinguished by a
/// stable sub-key, not by adding an enumerator per sensor. This is what lets the
/// identity scale to GPU, cooling, storage, and remote sources without the enum
/// exploding.
enum class MetricId { CpuTotalUtilization, MemoryUtilization, CpuTemperature };

enum class MetricUnit { Percent, Celsius };

/// Data-quality of a reading. This is NOT health: it says whether the value is
/// current, stale, or absent, and nothing about whether the value is good or
/// bad. Health (Normal / Medium / High / Critical) is a separate future axis
/// owned exclusively by the Health Engine; no service or UI computes it here.
enum class MetricState { Unavailable, Fresh, Stale };

using MonotonicTimestamp = std::int64_t;

/// Stable composite identity for a sensor: a category plus a stable string key
/// within that category (for example CpuTemperature + "package", or
/// CpuTemperature + "ccd1"). The key is empty for single-instance metrics whose
/// category already identifies them uniquely (CPU utilization, memory).
///
/// This identity is the join key across the whole telemetry domain: the live
/// MetricSample, the static SensorDefinition, and -- in future work -- monitoring
/// profiles, history series, health evaluation, AI analysis, and widgets. It is
/// therefore deliberately transport-independent: it says nothing about hwmon,
/// paths, or how the value is obtained.
struct SensorKey {
    MetricId category{MetricId::CpuTotalUtilization};
    std::string key{};

    friend bool operator==(const SensorKey&, const SensorKey&) = default;
};

class MetricSample {
public:
    static MetricSample unavailable(MetricId id, MonotonicTimestamp t,
                                    std::string key = {});
    static std::optional<MetricSample> tryFresh(MetricId id, double value,
                                                MetricUnit unit,
                                                MonotonicTimestamp t,
                                                std::string key = {});
    static std::optional<MetricSample> tryStale(MetricId id, double value,
                                                MetricUnit unit,
                                                MonotonicTimestamp t,
                                                std::string key = {});
    [[nodiscard]] MetricId id() const { return id_; }
    [[nodiscard]] const std::string& sensorKey() const { return sensorKey_; }
    /// Full composite identity (category + key).
    [[nodiscard]] SensorKey identity() const {
        return SensorKey{id_, sensorKey_};
    }
    [[nodiscard]] std::optional<double> value() const { return value_; }
    [[nodiscard]] MetricUnit unit() const { return unit_; }
    [[nodiscard]] MetricState state() const { return state_; }
    [[nodiscard]] MonotonicTimestamp timestamp() const { return timestamp_; }
    friend bool operator==(const MetricSample&, const MetricSample&) = default;
private:
    MetricSample() = default;
    MetricId id_{MetricId::CpuTotalUtilization};
    std::string sensorKey_{};
    std::optional<double> value_{};
    MetricUnit unit_{MetricUnit::Percent};
    MetricState state_{MetricState::Unavailable};
    MonotonicTimestamp timestamp_{0};
};
}
#endif
