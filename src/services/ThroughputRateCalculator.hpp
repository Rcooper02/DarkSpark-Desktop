// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_SERVICES_THROUGHPUTRATECALCULATOR_HPP
#define DARKSPARK_SERVICES_THROUGHPUTRATECALCULATOR_HPP

#include <cstdint>
#include <optional>

namespace darkspark::services {

/// Computes a bytes-per-second rate from a monotonically increasing cumulative
/// byte counter sampled over time. Pure and Qt-free so it is deterministically
/// testable; the DiskStatsProvider owns one of these per counter (read / write).
///
/// Honesty rules (decision: never fabricate throughput):
///   - The FIRST update returns nullopt -- there is no previous sample to diff,
///     so throughput is genuinely Unavailable, never a fabricated 0.
///   - A counter DECREASE (device reset, counter wrap, unplug/replug) returns
///     nullopt and re-primes -- a negative or bogus rate is never reported.
///   - A non-positive elapsed time returns nullopt (avoids divide-by-zero and a
///     meaningless spike); the sample re-primes.
/// On a normal update it returns (deltaBytes / elapsedSeconds).
class ThroughputRateCalculator {
public:
    /// Feed a new cumulative byte count and the timestamp (milliseconds) it was
    /// read at. Returns bytes/sec, or nullopt when a rate cannot honestly be
    /// computed (first sample, counter reset, non-positive interval).
    [[nodiscard]] std::optional<double> update(std::uint64_t cumulativeBytes,
                                               std::int64_t timestampMs) {
        if (!primed_) {
            prevBytes_ = cumulativeBytes;
            prevMs_ = timestampMs;
            primed_ = true;
            return std::nullopt;  // first sample: honestly Unavailable
        }
        if (cumulativeBytes < prevBytes_ || timestampMs <= prevMs_) {
            // Counter reset/wrap or non-advancing clock: re-prime, report none.
            prevBytes_ = cumulativeBytes;
            prevMs_ = timestampMs;
            return std::nullopt;
        }
        const std::uint64_t deltaBytes = cumulativeBytes - prevBytes_;
        const double elapsedSec =
            static_cast<double>(timestampMs - prevMs_) / 1000.0;
        prevBytes_ = cumulativeBytes;
        prevMs_ = timestampMs;
        if (elapsedSec <= 0.0) {
            return std::nullopt;
        }
        return static_cast<double>(deltaBytes) / elapsedSec;
    }

    /// Forget prior state so the next update() is treated as a first sample.
    void reset() { primed_ = false; }

private:
    std::uint64_t prevBytes_ = 0;
    std::int64_t prevMs_ = 0;
    bool primed_ = false;
};

}  // namespace darkspark::services

#endif  // DARKSPARK_SERVICES_THROUGHPUTRATECALCULATOR_HPP
