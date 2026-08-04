// SPDX-License-Identifier: GPL-3.0-or-later
#include "services/GpuDeviceSelector.hpp"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <algorithm>

namespace darkspark::services {

namespace {

/// Count PCI address segments (e.g. "0000:03:00.0") in a resolved device path.
/// Integrated GPUs are shallow root-complex endpoints; discrete GPUs sit behind
/// PCIe bridges and yield a deeper path. This is the general, hardware-agnostic
/// discriminator used to prefer discrete over integrated.
int pcieDepthOf(const QString& resolvedPath) {
    int depth = 0;
    const QStringList parts = resolvedPath.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (const QString& p : parts) {
        // A PCI address segment looks like DOMAIN:BUS:DEV.FUNC, e.g.
        // "0000:03:00.0": two colons and a dot. Match that shape.
        const qsizetype firstColon = p.indexOf(QLatin1Char(':'));
        if (firstColon <= 0) {
            continue;
        }
        const qsizetype secondColon = p.indexOf(QLatin1Char(':'), firstColon + 1);
        if (secondColon > firstColon && p.contains(QLatin1Char('.'))) {
            ++depth;
        }
    }
    return depth;
}

QString canonical(const QString& path) {
    const QFileInfo fi(path);
    const QString c = fi.canonicalFilePath();
    return c.isEmpty() ? path : c;
}

std::optional<QString> readText(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return std::nullopt;
    }
    const QString s = QString::fromUtf8(f.readAll()).trimmed();
    return s.isEmpty() ? std::nullopt : std::optional<QString>(s);
}

// --- Production sysfs enumeration -------------------------------------------

std::vector<GpuCandidate> enumerateCandidatesProd() {
    std::vector<GpuCandidate> out;
    QDir drm(QStringLiteral("/sys/class/drm"));
    const QStringList cards =
        drm.entryList(QStringList{QStringLiteral("card*")}, QDir::Dirs, QDir::Name);
    for (const QString& card : cards) {
        if (card.contains(QLatin1Char('-'))) {
            continue;  // connector node like card0-DP-1
        }
        const QString devDir = drm.absoluteFilePath(card + QStringLiteral("/device"));
        // Must be an amdgpu device with a readable gpu_busy_percent.
        const QString busy = devDir + QStringLiteral("/gpu_busy_percent");
        if (!QFile::exists(busy)) {
            continue;
        }
        // Confirm the driver is amdgpu (driver symlink basename).
        const QString driverLink = devDir + QStringLiteral("/driver");
        const QString driver = QFileInfo(driverLink).canonicalFilePath();
        if (!driver.endsWith(QStringLiteral("amdgpu"))) {
            continue;
        }
        GpuCandidate c;
        c.drmCardPath = drm.absoluteFilePath(card);
        c.resolvedDevicePath = canonical(devDir);
        c.busyPercentPath = busy;
        c.pcieDepth = pcieDepthOf(c.resolvedDevicePath);
        out.push_back(c);
    }
    return out;
}

std::vector<GpuHwmonCandidate> enumerateHwmonProd() {
    std::vector<GpuHwmonCandidate> out;
    QDir hwmon(QStringLiteral("/sys/class/hwmon"));
    const QStringList entries =
        hwmon.entryList(QStringList{QStringLiteral("hwmon*")}, QDir::Dirs, QDir::Name);
    for (const QString& e : entries) {
        const QString dir = hwmon.absoluteFilePath(e);
        const std::optional<QString> name = readText(dir + QStringLiteral("/name"));
        if (!name || *name != QStringLiteral("amdgpu")) {
            continue;
        }
        GpuHwmonCandidate h;
        h.hwmonPath = dir;
        h.resolvedDevicePath = canonical(dir + QStringLiteral("/device"));
        out.push_back(h);
    }
    return out;
}

QString explicitOverrideProd() {
    // Deterministic override hook for future explicit GPU selection: an
    // environment variable naming the resolved device path to force. Empty when
    // unset, so production defaults to automatic discrete-preferred selection.
    const QByteArray env = qgetenv("DARKSPARK_GPU_DEVICE");
    return QString::fromUtf8(env).trimmed();
}

}  // namespace

namespace detail {

std::optional<GpuDeviceSelection> selectFrom(
    const std::vector<GpuCandidate>& candidates,
    const std::vector<GpuHwmonCandidate>& hwmons,
    const QString& explicitOverride) {
    if (candidates.empty()) {
        return std::nullopt;
    }

    const GpuCandidate* chosen = nullptr;

    // 1. Explicit override wins if it matches a candidate.
    if (!explicitOverride.isEmpty()) {
        for (const GpuCandidate& c : candidates) {
            if (c.resolvedDevicePath == explicitOverride) {
                chosen = &c;
                break;
            }
        }
    }

    // 2. Otherwise prefer greatest PCIe depth (discrete over integrated), with a
    //    stable tie-break on drmCardPath so selection is deterministic.
    if (chosen == nullptr) {
        for (const GpuCandidate& c : candidates) {
            if (chosen == nullptr || c.pcieDepth > chosen->pcieDepth
                || (c.pcieDepth == chosen->pcieDepth
                    && c.drmCardPath < chosen->drmCardPath)) {
                chosen = &c;
            }
        }
    }

    GpuDeviceSelection sel;
    sel.drmCardPath = chosen->drmCardPath;
    sel.resolvedDevicePath = chosen->resolvedDevicePath;
    sel.busyPercentPath = chosen->busyPercentPath;

    // 3. Join hwmon on the resolved device path so temperature binds to the SAME
    //    physical GPU as utilization. Missing hwmon is allowed.
    for (const GpuHwmonCandidate& h : hwmons) {
        if (h.resolvedDevicePath == chosen->resolvedDevicePath) {
            sel.hwmonPath = h.hwmonPath;
            sel.hasHwmon = true;
            break;
        }
    }
    return sel;
}

}  // namespace detail

std::optional<GpuDeviceSelection> selectGpuDevice(
    const detail::GpuDeviceSources& sources) {
    const std::vector<GpuCandidate> candidates =
        sources.enumerateCandidates ? sources.enumerateCandidates()
                                    : std::vector<GpuCandidate>{};
    const std::vector<GpuHwmonCandidate> hwmons =
        sources.enumerateHwmon ? sources.enumerateHwmon()
                               : std::vector<GpuHwmonCandidate>{};
    const QString override =
        sources.explicitDevicePath ? sources.explicitDevicePath() : QString();
    return detail::selectFrom(candidates, hwmons, override);
}

std::optional<GpuDeviceSelection> selectGpuDevice() {
    detail::GpuDeviceSources s;
    s.enumerateCandidates = enumerateCandidatesProd;
    s.enumerateHwmon = enumerateHwmonProd;
    s.explicitDevicePath = explicitOverrideProd;
    return selectGpuDevice(s);
}

}  // namespace darkspark::services
