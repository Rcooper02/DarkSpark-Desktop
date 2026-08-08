// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_SERVICES_NETWORKINTERFACEPROVIDER_HPP
#define DARKSPARK_SERVICES_NETWORKINTERFACEPROVIDER_HPP

#include <QString>

#include <cstdint>
#include <functional>
#include <optional>
#include <unordered_map>
#include <vector>

#include "services/NetworkSensorProvider.hpp"
#include "services/ThroughputRateCalculator.hpp"

namespace darkspark::services {

/// One raw snapshot of a network interface, as read from /proc/net/dev + sysfs +
/// the routing table. rxBytes/txBytes are cumulative since boot. The flags are
/// resolved by the enumerator (production) or injected (tests).
struct NetworkInterfaceSnapshot {
    QString name;                 ///< interface name, e.g. "enp5s0"
    std::uint64_t rxBytes = 0;    ///< cumulative received bytes
    std::uint64_t txBytes = 0;    ///< cumulative transmitted bytes
    bool isUp = false;            ///< operstate up
    bool isLoopback = false;      ///< the loopback interface
    bool isPhysical = false;      ///< a physical NIC (not virtual/bridge/veth/tun)
    bool isDefaultRoute = false;  ///< carries the default route
};

/// Reads network interfaces from /proc/net/dev (+ sysfs for classification and
/// /proc/net/route for the default route) and yields receive/transmit rate,
/// cumulative received/transmitted bytes, and link-state sensors.
///
/// This is the stateful provider: it owns a ThroughputRateCalculator per
/// interface per direction and computes rates across polls -- REUSING the exact
/// Storage rate machinery unchanged. The rate math lives ENTIRELY here; the
/// service just calls read(). Honest behaviour (never fabricate): first
/// observation Unavailable, counter reset -> Unavailable, non-positive interval
/// -> Unavailable, never negative.
///
/// The interface source and clock are injectable for deterministic tests.
class NetworkInterfaceProvider : public NetworkSensorProvider {
public:
    using InterfaceSource = std::function<std::vector<NetworkInterfaceSnapshot>()>;
    using Clock = std::function<std::int64_t()>;  // milliseconds

    NetworkInterfaceProvider();
    NetworkInterfaceProvider(InterfaceSource interfaces, Clock clock);

    [[nodiscard]] QString providerName() const override;
    [[nodiscard]] std::vector<NormalizedNetworkSensor> discover() const override;

private:
    InterfaceSource interfaces_;
    Clock clock_;

    // Per-interface rate state. discover() updates these each call. Mutable
    // because discover() is const by interface but must advance rate state.
    struct InterfaceRates {
        ThroughputRateCalculator rx;
        ThroughputRateCalculator tx;
        std::optional<double> lastRxRate;
        std::optional<double> lastTxRate;
    };
    mutable std::unordered_map<std::string, InterfaceRates> state_;
};

}  // namespace darkspark::services

#endif  // DARKSPARK_SERVICES_NETWORKINTERFACEPROVIDER_HPP
