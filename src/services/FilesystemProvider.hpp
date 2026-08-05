// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_SERVICES_FILESYSTEMPROVIDER_HPP
#define DARKSPARK_SERVICES_FILESYSTEMPROVIDER_HPP

#include <QString>

#include <cstdint>
#include <functional>
#include <vector>

#include "services/StorageSensorProvider.hpp"

namespace darkspark::services {

/// A discovered mounted filesystem, before it is turned into sensors. Exposed so
/// the service's filesystem-selection policy can rank candidates and so tests
/// can inject a synthetic mount table.
struct MountedFilesystem {
    QString mountPoint;    ///< e.g. "/"
    QString device;        ///< backing device, e.g. "/dev/nvme0n1p2"
    QString fsType;        ///< e.g. "ext4", "btrfs", "tmpfs"
    bool isLocal = false;  ///< a real local disk (not network/pseudo)
    bool isWritable = false;
    std::uint64_t totalBytes = 0;
    std::uint64_t usedBytes = 0;
};

/// Reads mounted filesystems and yields utilization/used/total sensors.
///
/// Production reads the mount table and statvfs; both are injected so tests can
/// drive a synthetic set of filesystems deterministically. Pseudo/network
/// filesystems (tmpfs, proc, sysfs, overlay, nfs, ...) are excluded from local
/// ranking but still reported if mounted, so selection stays deterministic.
class FilesystemProvider : public StorageSensorProvider {
public:
    using Enumerator = std::function<std::vector<MountedFilesystem>()>;

    /// Production constructor (reads /proc/self/mountinfo + statvfs).
    FilesystemProvider();
    /// Test constructor with an injected mount enumerator.
    explicit FilesystemProvider(Enumerator enumerator);

    [[nodiscard]] QString providerName() const override;
    [[nodiscard]] std::vector<NormalizedStorageSensor> discover() const override;

private:
    Enumerator enumerator_;
};

}  // namespace darkspark::services

#endif  // DARKSPARK_SERVICES_FILESYSTEMPROVIDER_HPP
