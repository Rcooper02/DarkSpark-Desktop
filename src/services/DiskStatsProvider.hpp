// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_SERVICES_DISKSTATSPROVIDER_HPP
#define DARKSPARK_SERVICES_DISKSTATSPROVIDER_HPP

#include <QString>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#include "services/StorageSensorProvider.hpp"
#include "services/ThroughputRateCalculator.hpp"

namespace darkspark::services {

/// One raw cumulative-counter reading for a block device from /proc/diskstats.
/// bytesRead/bytesWritten are cumulative since boot (sectors * 512).
struct DiskCounters {
    QString device;               ///< canonical block device, e.g. "nvme0n1"
    std::uint64_t bytesRead = 0;  ///< cumulative
    std::uint64_t bytesWritten = 0;  ///< cumulative
};

/// Reads /proc/diskstats and yields read/write THROUGHPUT (bytes/sec) sensors.
///
/// This is the stateful provider: it owns a ThroughputRateCalculator per device
/// per direction and computes rates across polls. The rate math lives ENTIRELY
/// here -- the service just calls read() and gets bytes/sec or nullopt. Honest
/// behavior (never fabricate): first observation Unavailable, counter reset ->
/// Unavailable, non-positive interval -> Unavailable, never a negative rate.
///
/// The counter source and clock are injectable for deterministic tests.
class DiskStatsProvider : public StorageSensorProvider {
public:
    using CounterSource = std::function<std::vector<DiskCounters>()>;
    using Clock = std::function<std::int64_t()>;  // milliseconds

    DiskStatsProvider();
    DiskStatsProvider(CounterSource counters, Clock clock);

    [[nodiscard]] QString providerName() const override;
    [[nodiscard]] std::vector<NormalizedStorageSensor> discover() const override;

private:
    CounterSource counters_;
    Clock clock_;

    // Per-device rate state. discover() updates these each call. Mutable because
    // discover() is const by interface but must advance rate state.
    struct DeviceRates {
        ThroughputRateCalculator read;
        ThroughputRateCalculator write;
        std::optional<double> lastReadRate;
        std::optional<double> lastWriteRate;
    };
    mutable std::unordered_map<std::string, DeviceRates> state_;
};

}  // namespace darkspark::services

#endif  // DARKSPARK_SERVICES_DISKSTATSPROVIDER_HPP
