// SPDX-License-Identifier: GPL-3.0-or-later
#include "deck/instruments/MemoryInstrumentModelAdapter.hpp"

#include <algorithm>

namespace darkspark::deck::instruments {

using models::MetricId;
using models::MetricSample;
using models::MetricState;

ValueAvailability MemoryInstrumentModelAdapter::availabilityFor(
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

void MemoryInstrumentModelAdapter::updateDerivedAvailable() {
    // Available is meaningful only when both used and total are present. It is
    // derived, never sourced: total - used, floored at zero to absorb any
    // transient inconsistency between two independently-timed samples.
    const bool bothPresent =
        model_.usedAvailability != ValueAvailability::Absent
        && model_.totalAvailability != ValueAvailability::Absent;
    if (!bothPresent) {
        model_.availableAvailability = ValueAvailability::Absent;
        return;
    }
    model_.availableBytes = std::max(0.0, model_.totalBytes - model_.usedBytes);
    // The derived value is only as fresh as its least-fresh input: if either
    // input is LastKnown, the derived available is LastKnown too.
    const bool eitherStale =
        model_.usedAvailability == ValueAvailability::LastKnown
        || model_.totalAvailability == ValueAvailability::LastKnown;
    model_.availableAvailability =
        eitherStale ? ValueAvailability::LastKnown : ValueAvailability::Live;
}

bool MemoryInstrumentModelAdapter::apply(const MetricSample& sample) {
    switch (sample.id()) {
    case MetricId::MemoryUtilization: {
        const ValueAvailability avail = availabilityFor(sample.state());
        if (sample.value().has_value()) {
            // Sanitize: utilization is a percentage; clamp out-of-range values
            // rather than trusting the source blindly.
            model_.utilizationPercent =
                std::clamp(sample.value().value(), 0.0, 100.0);
        }
        model_.utilizationAvailability = avail;
        return true;
    }
    case MetricId::MemoryUsedBytes: {
        const ValueAvailability avail = availabilityFor(sample.state());
        if (sample.value().has_value()) {
            // Bytes are non-negative; clamp defensively against a malformed
            // negative rather than propagating it.
            model_.usedBytes = std::max(0.0, sample.value().value());
        }
        model_.usedAvailability = avail;
        updateDerivedAvailable();
        return true;
    }
    case MetricId::MemoryTotalBytes: {
        const ValueAvailability avail = availabilityFor(sample.state());
        if (sample.value().has_value()) {
            model_.totalBytes = std::max(0.0, sample.value().value());
        }
        model_.totalAvailability = avail;
        updateDerivedAvailable();
        return true;
    }
    case MetricId::CpuTotalUtilization:
    case MetricId::CpuTemperature:
    case MetricId::GpuTotalUtilization:
    case MetricId::GpuTemperature:
    case MetricId::CoolingPrimary:
    case MetricId::CoolingSecondary:
    case MetricId::CoolingCoolantTemp:
        // Not shown by the memory instrument. Explicit cases (not a default)
        // preserve the -Wswitch guarantee.
        return false;
    }
    return false;
}

}  // namespace darkspark::deck::instruments
