// SPDX-License-Identifier: GPL-3.0-or-later
#include "deck/instruments/CoolingInstrumentModelAdapter.hpp"

#include <algorithm>

namespace darkspark::deck::instruments {

using models::MetricId;
using models::MetricSample;
using models::MetricState;

ValueAvailability CoolingInstrumentModelAdapter::availabilityFor(
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

bool CoolingInstrumentModelAdapter::apply(const MetricSample& sample) {
    switch (sample.id()) {
    case MetricId::CoolingPrimary: {
        const ValueAvailability avail = availabilityFor(sample.state());
        if (sample.value().has_value()) {
            model_.primaryRpm = std::max(0.0, sample.value().value());
        }
        model_.primaryAvailability = avail;
        return true;
    }
    case MetricId::CoolingSecondary: {
        const ValueAvailability avail = availabilityFor(sample.state());
        if (sample.value().has_value()) {
            model_.secondaryRpm = std::max(0.0, sample.value().value());
        }
        model_.secondaryRpmAvailability = avail;
        return true;
    }
    case MetricId::CoolingCoolantTemp: {
        const ValueAvailability avail = availabilityFor(sample.state());
        if (sample.value().has_value()) {
            model_.secondaryTempCelsius = sample.value().value();
        }
        model_.secondaryTempAvailability = avail;
        return true;
    }
    case MetricId::CpuTotalUtilization:
    case MetricId::CpuTemperature:
    case MetricId::GpuTotalUtilization:
    case MetricId::GpuTemperature:
    case MetricId::MemoryUtilization:
    case MetricId::MemoryUsedBytes:
    case MetricId::MemoryTotalBytes:
        // Not shown by the cooling instrument. Explicit cases (not a default)
        // preserve the -Wswitch guarantee.
        return false;
    }
    return false;
}

}  // namespace darkspark::deck::instruments
