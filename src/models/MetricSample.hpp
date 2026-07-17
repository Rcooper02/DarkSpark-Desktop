// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_MODELS_METRICSAMPLE_HPP
#define DARKSPARK_MODELS_METRICSAMPLE_HPP
#include <cstdint>
#include <optional>
namespace darkspark::models {
enum class MetricId { CpuTotalUtilization };
enum class MetricUnit { Percent };
enum class MetricState { Unavailable, Fresh, Stale };
using MonotonicTimestamp = std::int64_t;
class MetricSample {
public:
    static MetricSample unavailable(MetricId id, MonotonicTimestamp t);
    static std::optional<MetricSample> tryFresh(MetricId id, double value, MetricUnit unit, MonotonicTimestamp t);
    static std::optional<MetricSample> tryStale(MetricId id, double value, MetricUnit unit, MonotonicTimestamp t);
    [[nodiscard]] MetricId id() const { return id_; }
    [[nodiscard]] std::optional<double> value() const { return value_; }
    [[nodiscard]] MetricUnit unit() const { return unit_; }
    [[nodiscard]] MetricState state() const { return state_; }
    [[nodiscard]] MonotonicTimestamp timestamp() const { return timestamp_; }
    friend bool operator==(const MetricSample&, const MetricSample&) = default;
private:
    MetricSample() = default;
    MetricId id_{MetricId::CpuTotalUtilization};
    std::optional<double> value_{};
    MetricUnit unit_{MetricUnit::Percent};
    MetricState state_{MetricState::Unavailable};
    MonotonicTimestamp timestamp_{0};
};
}
#endif
