// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_SERVICES_STORAGESENSORPROVIDER_HPP
#define DARKSPARK_SERVICES_STORAGESENSORPROVIDER_HPP

#include <QString>

#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "services/SensorMetadata.hpp"

namespace darkspark::services {

/// What a discovered storage sensor measures. A provider assigns the kind; the
/// service maps kinds onto the role-based storage MetricIds.
enum class StorageSensorKind {
    FilesystemUtilization,  ///< percent used of a filesystem
    FilesystemUsedBytes,    ///< used bytes of a filesystem
    FilesystemTotalBytes,   ///< total bytes of a filesystem
    Temperature,            ///< NVMe/drive temperature (Celsius)
    ReadRate,               ///< disk read throughput (bytes/sec)
    WriteRate               ///< disk write throughput (bytes/sec)
};

/// The unit a normalized storage sensor reports in.
enum class StorageSensorUnit { Percent, Bytes, Celsius, BytesPerSecond };

/// Which logical target a sensor belongs to, so the service can group a
/// filesystem's sensors and a drive's sensors and apply the selection policies.
/// `selectionKey` is a stable grouping key (e.g. a mount point for a filesystem,
/// or a drive stableId); `backingDeviceHint` lets a filesystem sensor name the
/// block device that backs it, so the disk-selection policy can prefer the drive
/// behind the selected filesystem.
struct StorageTarget {
    QString selectionKey;       ///< stable group key (mount point, or drive id)
    QString backingDeviceHint;  ///< block device backing a filesystem, if known
    bool isRootFilesystem = false;   ///< true for "/"
    bool isWritableLocal = false;    ///< writable, local (not network/removable)
    double totalBytesForRanking = 0.0;  ///< for "largest writable local" tie-break
    QString filesystemType;     ///< e.g. "ext4"; for transparent discovery logging
    QString hwmonName;          ///< e.g. "hwmon2"; the drive's hwmon dir, for logging
};

/// A provider-independent storage sensor the service can read. Providers
/// translate their native sources (statvfs, hwmon, /proc/diskstats) into this
/// common shape, so the service consumes normalized objects and never cares
/// where they came from -- nor how a value is computed. In particular a
/// throughput sensor's `read` is STATEFUL inside the provider (it holds previous
/// counters + timestamp and computes a rate); the service just calls read() and
/// gets bytes/sec, or nullopt on the first poll / a counter reset. No value is
/// ever fabricated.
struct NormalizedStorageSensor {
    StorageSensorKind kind = StorageSensorKind::FilesystemUtilization;
    StorageSensorUnit unit = StorageSensorUnit::Percent;
    StorageTarget target;
    SensorMetadata metadata;
    /// Read the current value in the sensor's unit. Returns nullopt on a
    /// missing/malformed reading, or (for a rate) when no previous sample exists
    /// yet -- the service then reports Unavailable/Stale, never a fabricated 0.
    std::function<std::optional<double>()> read;
};

/// Interface every storage sensor source implements -- the same permanent
/// discovery pattern as Cooling. New sources (SMART, ZFS/btrfs pools, iostat,
/// removable/remote volumes, Windows) are added as new providers WITHOUT
/// changing the service, adapter, instrument, telemetry contract, or renderer.
class StorageSensorProvider {
public:
    virtual ~StorageSensorProvider() = default;

    /// A short provider identity for discovery logging, e.g. "filesystem".
    [[nodiscard]] virtual QString providerName() const = 0;

    /// Enumerate the storage sensors this provider can currently see, each
    /// normalized. Freshly resolved on each call; never caches unstable indices.
    [[nodiscard]] virtual std::vector<NormalizedStorageSensor> discover()
        const = 0;
};

using StorageSensorProviderPtr = std::shared_ptr<StorageSensorProvider>;

}  // namespace darkspark::services

#endif  // DARKSPARK_SERVICES_STORAGESENSORPROVIDER_HPP
