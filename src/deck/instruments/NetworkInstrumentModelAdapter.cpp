// SPDX-License-Identifier: GPL-3.0-or-later
#include "deck/instruments/NetworkInstrumentModelAdapter.hpp"

#include <algorithm>

namespace darkspark::deck::instruments {

using models::MetricId;
using models::MetricSample;
using models::MetricState;

ValueAvailability NetworkInstrumentModelAdapter::availabilityFor(
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

bool NetworkInstrumentModelAdapter::apply(const MetricSample& sample) {
    switch (sample.id()) {
    case MetricId::NetworkReceiveRate: {
        const ValueAvailability avail = availabilityFor(sample.state());
        if (sample.value().has_value()) {
            model_.receiveBytesPerSec = std::max(0.0, sample.value().value());
        }
        model_.receiveAvailability = avail;
        return true;
    }
    case MetricId::NetworkTransmitRate: {
        const ValueAvailability avail = availabilityFor(sample.state());
        if (sample.value().has_value()) {
            model_.transmitBytesPerSec = std::max(0.0, sample.value().value());
        }
        model_.transmitAvailability = avail;
        return true;
    }
    case MetricId::NetworkReceivedBytes: {
        const ValueAvailability avail = availabilityFor(sample.state());
        if (sample.value().has_value()) {
            model_.receivedBytes = std::max(0.0, sample.value().value());
        }
        model_.receivedAvailability = avail;
        return true;
    }
    case MetricId::NetworkTransmittedBytes: {
        const ValueAvailability avail = availabilityFor(sample.state());
        if (sample.value().has_value()) {
            model_.transmittedBytes = std::max(0.0, sample.value().value());
        }
        model_.transmittedAvailability = avail;
        return true;
    }
    case MetricId::NetworkLinkState: {
        const ValueAvailability avail = availabilityFor(sample.state());
        if (sample.value().has_value()) {
            // Interpreted by MetricId: 0 = Down, non-zero (1) = Up.
            model_.linkState = (sample.value().value() >= 0.5)
                                   ? NetworkLinkState::Up
                                   : NetworkLinkState::Down;
        }
        model_.linkAvailability = avail;
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
    case MetricId::StorageUtilization:
    case MetricId::StorageUsedBytes:
    case MetricId::StorageTotalBytes:
    case MetricId::StorageTemperature:
    case MetricId::StorageReadRate:
    case MetricId::StorageWriteRate:
        // Not shown by the network instrument. Explicit cases (not a default)
        // preserve the -Wswitch guarantee.
        return false;
    }
    return false;
}

}  // namespace darkspark::deck::instruments
