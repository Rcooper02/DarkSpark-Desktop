// SPDX-License-Identifier: GPL-3.0-or-later
#include "models/MetricSample.hpp"
#include <cmath>
#include <utility>
namespace darkspark::models {
MetricSample MetricSample::unavailable(MetricId id, MonotonicTimestamp t,
                                       std::string key) {
    MetricSample s;
    s.id_ = id;
    s.sensorKey_ = std::move(key);
    s.value_ = std::nullopt;
    s.state_ = MetricState::Unavailable;
    s.timestamp_ = t;
    return s;
}
std::optional<MetricSample> MetricSample::tryFresh(MetricId id, double value,
                                                   MetricUnit unit,
                                                   MonotonicTimestamp t,
                                                   std::string key) {
    if (!std::isfinite(value)) { return std::nullopt; }
    MetricSample s;
    s.id_ = id;
    s.sensorKey_ = std::move(key);
    s.value_ = value;
    s.unit_ = unit;
    s.state_ = MetricState::Fresh;
    s.timestamp_ = t;
    return s;
}
std::optional<MetricSample> MetricSample::tryStale(MetricId id, double value,
                                                   MetricUnit unit,
                                                   MonotonicTimestamp t,
                                                   std::string key) {
    if (!std::isfinite(value)) { return std::nullopt; }
    MetricSample s;
    s.id_ = id;
    s.sensorKey_ = std::move(key);
    s.value_ = value;
    s.unit_ = unit;
    s.state_ = MetricState::Stale;
    s.timestamp_ = t;
    return s;
}
}
