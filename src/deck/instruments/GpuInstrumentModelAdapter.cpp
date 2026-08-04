// SPDX-License-Identifier: GPL-3.0-or-later
#include "deck/instruments/GpuInstrumentModelAdapter.hpp"

#include <algorithm>

namespace darkspark::deck::instruments {

using models::MetricId;
using models::MetricSample;
using models::MetricState;

ValueAvailability GpuInstrumentModelAdapter::availabilityFor(MetricState state) {
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

bool GpuInstrumentModelAdapter::apply(const MetricSample& sample) {
    switch (sample.id()) {
    case MetricId::GpuTotalUtilization: {
        const ValueAvailability avail = availabilityFor(sample.state());
        double pct = model_.utilizationPercent;
        if (sample.value().has_value()) {
            // Sanitize: utilization is a percentage; clamp out-of-range values
            // rather than trusting the source blindly.
            pct = std::clamp(sample.value().value(), 0.0, 100.0);
        }
        model_.utilizationPercent = pct;
        model_.utilizationAvailability = avail;
        return true;
    }
    case MetricId::GpuTemperature: {
        // Only the primary GPU temperature sensor drives the instrument. Any
        // other GPU temperature key (a non-primary label) is ignored here.
        if (sample.sensorKey() != kPrimaryTempKey) {
            return false;
        }
        const ValueAvailability avail = availabilityFor(sample.state());
        if (sample.value().has_value()) {
            model_.temperatureCelsius = sample.value().value();
        }
        model_.temperatureAvailability = avail;
        return true;
    }
    case MetricId::CpuTotalUtilization:
    case MetricId::CpuTemperature:
    case MetricId::MemoryUtilization:
        // Not shown by the GPU instrument. Explicit cases (not a default)
        // preserve the -Wswitch guarantee.
        return false;
    }
    return false;
}

}  // namespace darkspark::deck::instruments
