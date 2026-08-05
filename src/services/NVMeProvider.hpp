// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_SERVICES_NVMEPROVIDER_HPP
#define DARKSPARK_SERVICES_NVMEPROVIDER_HPP

#include <QString>

#include <functional>
#include <optional>
#include <vector>

#include "services/StorageSensorProvider.hpp"

namespace darkspark::services {

/// A discovered NVMe (or other drive) temperature source, before it becomes a
/// sensor. Exposed so the service's disk-selection policy can rank drives and so
/// tests can inject a synthetic set. `blockDevice` is the canonical block device
/// name (e.g. "nvme0n1") used to match against the filesystem's backing device.
struct DriveTemperatureSource {
    QString blockDevice;   ///< e.g. "nvme0n1"; empty if it cannot be determined
    QString stableId;      ///< stable identity, e.g. "nvme:Samsung_990_PRO:nvme0"
    QString model;         ///< drive model if exposed
    QString inputPath;     ///< hwmon tempN_input path (millidegrees C)
    bool isNvme = false;   ///< true if this is an NVMe device
    QString hwmonName;     ///< the hwmon dir name, e.g. "hwmon2", for logging
};

/// Reads drive (NVMe) temperature from hwmon.
///
/// Discovery discipline mirrors HwmonCoolingProvider / HwmonDiscovery: identify
/// devices by their `name` file, re-resolve every call, never cache hwmonN
/// indices. Device identity is derived from the hwmon device's backing block
/// device where determinable, so the disk-selection policy can prefer the drive
/// backing the selected filesystem.
///
/// HONEST MAPPING LIMIT: hwmon does not always expose a clean hwmon->block-device
/// link. When the block device cannot be determined, `blockDevice` is left empty
/// and the drive simply cannot be matched to a filesystem's backing device; the
/// service then falls back to its deterministic primary-NVMe policy rather than
/// guessing.
class NVMeProvider : public StorageSensorProvider {
public:
    using Enumerator = std::function<std::vector<DriveTemperatureSource>()>;

    NVMeProvider();
    explicit NVMeProvider(Enumerator enumerator);

    [[nodiscard]] QString providerName() const override;
    [[nodiscard]] std::vector<NormalizedStorageSensor> discover() const override;

private:
    Enumerator enumerator_;
};

}  // namespace darkspark::services

#endif  // DARKSPARK_SERVICES_NVMEPROVIDER_HPP
