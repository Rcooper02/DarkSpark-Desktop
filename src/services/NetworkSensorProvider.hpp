// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_SERVICES_NETWORKSENSORPROVIDER_HPP
#define DARKSPARK_SERVICES_NETWORKSENSORPROVIDER_HPP

#include <QString>

#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "services/SensorMetadata.hpp"

namespace darkspark::services {

/// What a discovered network sensor measures. A provider assigns the kind; the
/// service maps kinds onto the role-based network MetricIds.
enum class NetworkSensorKind {
    ReceiveRate,        ///< download throughput (bytes/sec)
    TransmitRate,       ///< upload throughput (bytes/sec)
    ReceivedBytes,      ///< cumulative received bytes
    TransmittedBytes,   ///< cumulative transmitted bytes
    LinkState           ///< 0 = down, 1 = up (numeric; no dedicated unit)
};

/// The unit a normalized network sensor reports in. Link state is carried as a
/// plain numeric value on Count (0/1) -- there is deliberately NO MetricUnit for
/// link state; the adapter and instrument interpret it by MetricId.
enum class NetworkSensorUnit { BytesPerSecond, Bytes, Count };

/// Which interface a sensor belongs to, plus the identity the selection policy
/// and discovery summary need. `selectionKey` is the interface name (e.g.
/// "enp5s0"); the flags classify it deterministically so the policy never
/// depends on enumeration order and never picks loopback unless overridden.
struct NetworkTarget {
    QString selectionKey;        ///< interface name, e.g. "enp5s0"
    bool isDefaultRoute = false; ///< carries the default route
    bool isLoopback = false;     ///< the loopback interface
    bool isPhysical = false;     ///< a physical NIC (not virtual/bridge/veth/tun)
    bool isUp = false;           ///< operstate up (link present)
};

/// A provider-independent network sensor the service can read. Providers
/// translate their native sources (/proc/net/dev, sysfs, /proc/net/route) into
/// this common shape, so the service consumes normalized objects and never cares
/// where they came from -- nor how a value is computed. In particular a rate
/// sensor's `read` is STATEFUL inside the provider (it holds previous counters +
/// timestamp via ThroughputRateCalculator and computes a rate); the service just
/// calls read() and gets bytes/sec, or nullopt on the first poll / a counter
/// reset. No value is ever fabricated.
struct NormalizedNetworkSensor {
    NetworkSensorKind kind = NetworkSensorKind::ReceiveRate;
    NetworkSensorUnit unit = NetworkSensorUnit::BytesPerSecond;
    NetworkTarget target;
    SensorMetadata metadata;
    /// Read the current value in the sensor's unit. Returns nullopt on a
    /// missing/malformed reading, or (for a rate) when no previous sample exists
    /// yet -- the service then reports Unavailable/Stale, never a fabricated 0.
    std::function<std::optional<double>()> read;
};

/// Interface every network sensor source implements -- the same permanent
/// discovery pattern as Cooling and Storage. New sources (NetworkManager, Wi-Fi
/// signal, VPN, latency, Windows, remote-server) are added as new providers
/// WITHOUT changing the service, adapter, instrument, telemetry contract, or
/// renderer. The /proc/net/dev interface provider is simply the first.
class NetworkSensorProvider {
public:
    virtual ~NetworkSensorProvider() = default;

    /// A short provider identity for discovery logging, e.g. "interface".
    [[nodiscard]] virtual QString providerName() const = 0;

    /// Enumerate the network sensors this provider can currently see, each
    /// normalized. Freshly resolved on each call; never caches unstable order.
    [[nodiscard]] virtual std::vector<NormalizedNetworkSensor> discover()
        const = 0;
};

using NetworkSensorProviderPtr = std::shared_ptr<NetworkSensorProvider>;

}  // namespace darkspark::services

#endif  // DARKSPARK_SERVICES_NETWORKSENSORPROVIDER_HPP
