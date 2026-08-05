// SPDX-License-Identifier: GPL-3.0-or-later
#include "deck/instruments/StorageInstrumentModelAdapter.hpp"

#include <algorithm>

namespace darkspark::deck::instruments {

using models::MetricId;
using models::MetricSample;
using models::MetricState;

ValueAvailability StorageInstrumentModelAdapter::availabilityFor(
    MetricState state) {
    switch (state) {
    case MetricState::Fresh:
        return ValueAvailability::Live;
    case MetricState::Stale:
        return ValueAvailability::LastKnown;
    case MetricState::Unavailable:
        return ValueAvailability::Absent;
    }
    return ValueAvailability::Absent;
}

bool StorageInstrumentModelAdapter::apply(const MetricSample& sample) {
    switch (sample.id()) {
    case MetricId::StorageUtilization: {
        const ValueAvailability avail = availabilityFor(sample.state());
        if (sample.value().has_value()) {
            model_.utilizationPercent =
                std::clamp(sample.value().value(), 0.0, 100.0);
        }
        model_.utilizationAvailability = avail;
        return true;
    }
    case MetricId::StorageUsedBytes: {
        const ValueAvailability avail = availabilityFor(sample.state());
        if (sample.value().has_value()) {
            model_.usedBytes = std::max(0.0, sample.value().value());
        }
        model_.usedAvailability = avail;
        return true;
    }
    case MetricId::StorageTotalBytes: {
        const ValueAvailability avail = availabilityFor(sample.state());
        if (sample.value().has_value()) {
            model_.totalBytes = std::max(0.0, sample.value().value());
        }
        model_.totalAvailability = avail;
        return true;
    }
    case MetricId::StorageTemperature: {
        const ValueAvailability avail = availabilityFor(sample.state());
        if (sample.value().has_value()) {
            model_.temperatureCelsius = sample.value().value();
        }
        model_.temperatureAvailability = avail;
        return true;
    }
    case MetricId::StorageReadRate: {
        const ValueAvailability avail = availabilityFor(sample.state());
        if (sample.value().has_value()) {
            model_.readBytesPerSec = std::max(0.0, sample.value().value());
        }
        model_.readAvailability = avail;
        return true;
    }
    case MetricId::StorageWriteRate: {
        const ValueAvailability avail = availabilityFor(sample.state());
        if (sample.value().has_value()) {
            model_.writeBytesPerSec = std::max(0.0, sample.value().value());
        }
        model_.writeAvailability = avail;
        return true;
    }
    case MetricId::CpuTotalUtilization:
    case MetricId::CpuTemperature:
    case MetricId::GpuTotalUtilization:
    case MetricId::GpuTemperature:
    case MetricId::MemoryUtilization:
    case MetricId::MemoryUsedBytes:
    case MetricId::MemoryTotalBytes:
    case MetricId::CoolingPrimary:
    case MetricId::CoolingSecondary:
    case MetricId::CoolingCoolantTemp:
        // Not shown by the storage instrument. Explicit cases (not a default)
        // preserve the -Wswitch guarantee.
        return false;
    }
    return false;
}

}  // namespace darkspark::deck::instruments
