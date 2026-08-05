// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_SERVICES_STORAGETELEMETRYSERVICE_HPP
#define DARKSPARK_SERVICES_STORAGETELEMETRYSERVICE_HPP

#include <QTimer>

#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "interfaces/ITelemetryProvider.hpp"
#include "models/MetricSample.hpp"
#include "services/StorageSensorProvider.hpp"

namespace darkspark::services {

// Forward-declared in the services namespace (not in detail) so detail helpers
// name services::StorageTelemetryService.
class StorageTelemetryService;

namespace detail {

/// A discovered sensor tagged with its producing provider, for logging.
struct AttributedStorageSensor {
    QString providerName;
    NormalizedStorageSensor sensor;
};

/// The outcome of the two deterministic selection policies.
///   selectedFilesystemKey : the mount point chosen by the filesystem policy
///   selectedDriveKey      : the drive chosen by the disk policy
///   driveMatchedFilesystem: true if the drive was chosen because it backs the
///                           selected filesystem (vs. the deterministic fallback)
struct StorageSelection {
    QString selectedFilesystemKey;
    QString selectedDriveKey;
    bool driveMatchedFilesystem = false;
};

/// The transparent discovery summary: the resolved identity behind a selection.
/// This is the SAME information the service logs at startup, exposed as data so
/// the discovery contract is testable (and cannot silently disappear in a
/// refactor). It carries no presentation and drives no UI -- logDiscovery reads
/// it to produce the log text, and tests assert on it directly.
struct DiscoverySummary {
    QString selectedFilesystem;   ///< selected mount point
    QString backingDevice;        ///< backing block device, empty if undetermined
    QString filesystemType;       ///< e.g. "ext4"
    double capacityBytes = 0.0;   ///< total capacity of the selected filesystem
    QString selectedNvmeHwmon;    ///< hwmon dir of the selected drive, if any
    QString diskStatsDevice;      ///< diskstats device of the selected drive
    bool usedDeterministicFallback = false;  ///< drive chosen by fallback, not match
};

/// Build the discovery summary from the aggregated sensors + the selection.
/// Pure and deterministic; the single source of the identity the service both
/// logs and (via tests) guarantees. Reads only what the providers supplied.
[[nodiscard]] DiscoverySummary summarizeDiscovery(
    const std::vector<AttributedStorageSensor>& sensors,
    const StorageSelection& selection);

/// Filesystem-selection policy (deterministic, order-independent):
///   1. explicit override (future seam; `override` when non-empty and present)
///   2. root filesystem "/"
///   3. largest writable local filesystem
///   4. first valid local filesystem (lexicographic by key) as final fallback
/// Returns the chosen filesystem selectionKey, or empty if none exist.
[[nodiscard]] QString selectFilesystem(
    const std::vector<AttributedStorageSensor>& sensors,
    const QString& overrideKey);

/// Disk-selection policy (deterministic):
///   1. the drive whose backing device matches the selected filesystem's device
///   2. deterministic primary NVMe fallback (lexicographically-first nvme drive)
///   3. else the lexicographically-first drive with a temperature sensor
/// Returns the chosen drive selectionKey (+ whether it matched the filesystem).
[[nodiscard]] StorageSelection selectDriveFor(
    const std::vector<AttributedStorageSensor>& sensors,
    const QString& filesystemKey, const QString& filesystemBackingDevice);

/// Test seam: build a service over explicit providers and a clock.
[[nodiscard]] StorageTelemetryService* makeWithProviders(
    std::vector<StorageSensorProviderPtr> providers,
    std::function<models::MonotonicTimestamp()> now, QObject* parent);
void pollOnceForTest(StorageTelemetryService& service);

}  // namespace detail

/// Aggregates storage telemetry across FilesystemProvider, NVMeProvider, and
/// DiskStatsProvider, applies the deterministic filesystem- and disk-selection
/// policies, and emits the six approved role-based metrics. It is a SELECTOR and
/// EMITTER only: presentation-free (no colours, strings-for-users, rings,
/// formatting, or progress), and it never computes rates (the DiskStatsProvider
/// owns that). Never fabricates: missing/malformed/first-rate reads become
/// Unavailable (or Stale if a prior value existed).
///
/// Emits: StorageUtilization (Percent), StorageUsedBytes (Bytes),
/// StorageTotalBytes (Bytes), StorageTemperature (Celsius),
/// StorageReadRate (BytesPerSecond), StorageWriteRate (BytesPerSecond).
class StorageTelemetryService : public interfaces::ITelemetryProvider {
    Q_OBJECT

public:
    explicit StorageTelemetryService(QObject* parent = nullptr);

    void start() override;
    void stop() override;
    [[nodiscard]] QList<models::MetricSample> currentSamples() const override;

private:
    friend StorageTelemetryService* detail::makeWithProviders(
        std::vector<StorageSensorProviderPtr> providers,
        std::function<models::MonotonicTimestamp()> now, QObject* parent);
    friend void detail::pollOnceForTest(StorageTelemetryService& service);

    StorageTelemetryService(std::vector<StorageSensorProviderPtr> providers,
                            std::function<models::MonotonicTimestamp()> now,
                            QObject* parent);

    void poll();
    void logDiscovery(const std::vector<detail::AttributedStorageSensor>& all,
                      const detail::StorageSelection& sel) const;
    void emitMetric(
        models::MetricId id, models::MetricUnit unit,
        const std::optional<detail::AttributedStorageSensor>& sensor,
        std::optional<models::MetricSample>& lastSample,
        std::optional<double>& lastValue);

    std::vector<StorageSensorProviderPtr> providers_;
    std::function<models::MonotonicTimestamp()> now_;
    QTimer* timer_ = nullptr;
    bool loggedDiscovery_ = false;
    QString overrideFilesystemKey_;  // future explicit-override seam; empty in V1

    // Last-known per-metric, for Stale handling + priming.
    std::optional<models::MetricSample> utilization_;
    std::optional<models::MetricSample> usedBytes_;
    std::optional<models::MetricSample> totalBytes_;
    std::optional<models::MetricSample> temperature_;
    std::optional<models::MetricSample> readRate_;
    std::optional<models::MetricSample> writeRate_;
    std::optional<double> lastUtil_;
    std::optional<double> lastUsed_;
    std::optional<double> lastTotal_;
    std::optional<double> lastTemp_;
    std::optional<double> lastRead_;
    std::optional<double> lastWrite_;
};

}  // namespace darkspark::services

#endif  // DARKSPARK_SERVICES_STORAGETELEMETRYSERVICE_HPP
