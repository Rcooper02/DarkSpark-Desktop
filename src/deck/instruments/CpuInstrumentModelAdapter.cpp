// SPDX-License-Identifier: GPL-3.0-or-later
#include "deck/instruments/CpuInstrumentModelAdapter.hpp"

#include <algorithm>

namespace darkspark::deck::instruments {

using models::MetricId;
using models::MetricSample;
using models::MetricState;

ValueAvailability CpuInstrumentModelAdapter::availabilityFor(MetricState state) {
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

bool CpuInstrumentModelAdapter::apply(const MetricSample& sample) {
    switch (sample.id()) {
    case MetricId::CpuTotalUtilization: {
        const ValueAvailability avail = availabilityFor(sample.state());
        double pct = model_.utilizationPercent;
        if (sample.value().has_value()) {
            // Sanitize: utilization is a percentage; clamp out-of-range values
            // rather than trusting the source blindly.
            pct = std::clamp(sample.value().value(), 0.0, 100.0);
        }
        // On Absent with no value, keep the last numeric value but mark Absent
        // so the instrument shows a placeholder without losing history.
        model_.utilizationPercent = pct;
        model_.utilizationAvailability = avail;
        return true;
    }
    case MetricId::CpuTemperature: {
        // Only the package sensor drives the instrument's temperature. CCD
        // samples (other keys) are intentionally ignored in this milestone.
        if (sample.sensorKey() != kPackageKey) {
            return false;
        }
        const ValueAvailability avail = availabilityFor(sample.state());
        if (sample.value().has_value()) {
            model_.temperatureCelsius = sample.value().value();
        }
        model_.temperatureAvailability = avail;
        return true;
    }
    case MetricId::MemoryUtilization:
        // Not shown by the CPU instrument.
        return false;
    }
    return false;
}

}  // namespace darkspark::deck::instruments
