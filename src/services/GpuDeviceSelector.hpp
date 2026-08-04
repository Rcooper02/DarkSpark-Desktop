// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_SERVICES_GPUDEVICESELECTOR_HPP
#define DARKSPARK_SERVICES_GPUDEVICESELECTOR_HPP

#include <QString>

#include <functional>
#include <optional>
#include <vector>

namespace darkspark::services {

/// The single, shared result of choosing which physical GPU the deck reads.
///
/// Both the utilization service (DRM gpu_busy_percent) and the thermal service
/// (amdgpu hwmon) consult this ONE selection so they always bind to the SAME
/// physical GPU. The join key is the resolved PCI device path: a DRM card and a
/// hwmon device belong to the same GPU iff their `device` symlinks canonicalize
/// to the same path.
struct GpuDeviceSelection {
    QString drmCardPath;         ///< e.g. /sys/class/drm/card1
    QString resolvedDevicePath;  ///< canonical PCI path, the join key
    QString busyPercentPath;     ///< drmCardPath + /device/gpu_busy_percent
    QString hwmonPath;           ///< matching hwmon dir, empty if none found
    bool hasHwmon = false;       ///< temperature is optional; utilization is not
};

/// One candidate AMD GPU discovered on the system, before selection. Kept
/// transport-independent so the selection RULE can be unit-tested without sysfs.
struct GpuCandidate {
    QString drmCardPath;         ///< /sys/class/drm/cardN
    QString resolvedDevicePath;  ///< canonicalized cardN/device
    QString busyPercentPath;     ///< readable gpu_busy_percent path
    /// PCIe topology depth: the number of PCI address segments in the resolved
    /// device path. Integrated GPUs are shallow (root-complex endpoints);
    /// discrete GPUs sit behind PCIe bridges and are deeper. Higher = more
    /// likely discrete.
    int pcieDepth = 0;
};

/// One discovered amdgpu hwmon device, before it is joined to a GPU.
struct GpuHwmonCandidate {
    QString hwmonPath;           ///< /sys/class/hwmon/hwmonN
    QString resolvedDevicePath;  ///< canonicalized hwmonN/device, the join key
};

namespace detail {

/// Injected collaborators so the whole selection is deterministic in tests.
///
/// `enumerateCandidates` returns the AMD GPU candidates (production: scans
/// /sys/class/drm/card*, keeps amdgpu cards with a readable gpu_busy_percent,
/// resolves each device path and computes its PCIe depth). `enumerateHwmon`
/// returns amdgpu hwmon devices with their resolved device paths.
/// `explicitDevicePath`, when it returns a non-empty string, forces selection
/// of the candidate whose resolvedDevicePath matches it -- the deterministic
/// override for future explicit GPU selection (e.g. from config or an env var).
struct GpuDeviceSources {
    std::function<std::vector<GpuCandidate>()> enumerateCandidates;
    std::function<std::vector<GpuHwmonCandidate>()> enumerateHwmon;
    std::function<QString()> explicitDevicePath;
};

/// The pure selection rule, exposed for testing. Given candidates and hwmon
/// devices (and an optional explicit override), choose one GPU and join its
/// hwmon. Returns nullopt only when there are no candidates at all.
///
/// Rule:
///   1. If `explicitOverride` is non-empty and matches a candidate's resolved
///      device path, that candidate wins (deterministic override).
///   2. Otherwise prefer the candidate with the greatest PCIe depth (discrete
///      over integrated). Ties break by drmCardPath for determinism.
///   3. Join hwmon by matching resolved device paths. If none matches,
///      hasHwmon is false but the selection is still valid (utilization works).
[[nodiscard]] std::optional<GpuDeviceSelection> selectFrom(
    const std::vector<GpuCandidate>& candidates,
    const std::vector<GpuHwmonCandidate>& hwmons,
    const QString& explicitOverride);

}  // namespace detail

/// Resolve the shared GPU selection using production sysfs sources. Returns
/// nullopt if no AMD GPU with readable utilization is present.
[[nodiscard]] std::optional<GpuDeviceSelection> selectGpuDevice();

/// Resolve using injected sources (deterministic tests / explicit override).
[[nodiscard]] std::optional<GpuDeviceSelection> selectGpuDevice(
    const detail::GpuDeviceSources& sources);

}  // namespace darkspark::services

#endif  // DARKSPARK_SERVICES_GPUDEVICESELECTOR_HPP
