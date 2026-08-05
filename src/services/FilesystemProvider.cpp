// SPDX-License-Identifier: GPL-3.0-or-later
#include "services/FilesystemProvider.hpp"

#include <sys/statvfs.h>

#include <QFile>

#include <array>

namespace darkspark::services {

namespace {

// Filesystem types that are not real local disks; excluded from local ranking.
bool isPseudoOrNetwork(const QString& fsType) {
    static const std::array<const char*, 14> kNonLocal = {
        "tmpfs",  "devtmpfs", "proc",     "sysfs",  "cgroup", "cgroup2",
        "overlay", "squashfs", "nfs",     "nfs4",   "cifs",   "fuse.gvfsd-fuse",
        "autofs", "debugfs"};
    for (const char* t : kNonLocal) {
        if (fsType == QLatin1String(t)) {
            return true;
        }
    }
    return false;
}

// Production mount enumeration: parse /proc/self/mountinfo and statvfs each
// mount. Kept out of the class so the class stays test-injectable.
std::vector<MountedFilesystem> enumerateProd() {
    std::vector<MountedFilesystem> out;
    QFile f(QStringLiteral("/proc/self/mountinfo"));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return out;
    }
    const QString content = QString::fromUtf8(f.readAll());
    const QStringList lines = content.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString& line : lines) {
        // mountinfo: fields separated by spaces, with " - " separating the mount
        // fields from the fs-type/source. Format (simplified):
        //   id parent maj:min root mountPoint options... - fsType source superOpts
        const qsizetype sep = line.indexOf(QStringLiteral(" - "));
        if (sep < 0) {
            continue;
        }
        const QStringList pre =
            line.left(sep).split(QLatin1Char(' '), Qt::SkipEmptyParts);
        const QStringList post =
            line.mid(sep + 3).split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (pre.size() < 6 || post.isEmpty()) {
            continue;
        }
        MountedFilesystem m;
        m.mountPoint = pre.at(4);
        m.fsType = post.at(0);
        m.device = post.size() > 1 ? post.at(1) : QString();
        m.isLocal = !isPseudoOrNetwork(m.fsType) && m.device.startsWith(QLatin1Char('/'));

        struct statvfs vfs {};
        if (statvfs(m.mountPoint.toUtf8().constData(), &vfs) == 0
            && vfs.f_blocks > 0) {
            const std::uint64_t frsize = vfs.f_frsize ? vfs.f_frsize : vfs.f_bsize;
            m.totalBytes = static_cast<std::uint64_t>(vfs.f_blocks) * frsize;
            const std::uint64_t freeBytes =
                static_cast<std::uint64_t>(vfs.f_bfree) * frsize;
            m.usedBytes = m.totalBytes >= freeBytes ? m.totalBytes - freeBytes : 0;
            m.isWritable = (vfs.f_flag & ST_RDONLY) == 0;
        }
        if (m.totalBytes > 0) {
            out.push_back(m);
        }
    }
    return out;
}

}  // namespace

FilesystemProvider::FilesystemProvider() : enumerator_(enumerateProd) {}

FilesystemProvider::FilesystemProvider(Enumerator enumerator)
    : enumerator_(std::move(enumerator)) {}

QString FilesystemProvider::providerName() const {
    return QStringLiteral("filesystem");
}

std::vector<NormalizedStorageSensor> FilesystemProvider::discover() const {
    std::vector<NormalizedStorageSensor> out;
    const std::vector<MountedFilesystem> mounts =
        enumerator_ ? enumerator_() : std::vector<MountedFilesystem>{};
    for (const MountedFilesystem& m : mounts) {
        StorageTarget target;
        target.selectionKey = m.mountPoint;
        target.backingDeviceHint = m.device;
        target.isRootFilesystem = (m.mountPoint == QStringLiteral("/"));
        target.isWritableLocal = m.isLocal && m.isWritable;
        target.totalBytesForRanking = static_cast<double>(m.totalBytes);
        target.filesystemType = m.fsType;

        SensorMetadata meta;
        meta.label = m.mountPoint;
        meta.stableId = QStringLiteral("fs:") + m.mountPoint;
        meta.devicePath = m.device;
        meta.capabilities = QStringList{QStringLiteral("utilization"),
                                        QStringLiteral("used"),
                                        QStringLiteral("total")};

        const double total = static_cast<double>(m.totalBytes);
        const double used = static_cast<double>(m.usedBytes);
        const double utilPct = total > 0.0 ? (used / total) * 100.0 : 0.0;

        NormalizedStorageSensor u;
        u.kind = StorageSensorKind::FilesystemUtilization;
        u.unit = StorageSensorUnit::Percent;
        u.target = target;
        u.metadata = meta;
        u.read = [utilPct]() { return std::optional<double>(utilPct); };
        out.push_back(u);

        NormalizedStorageSensor ub;
        ub.kind = StorageSensorKind::FilesystemUsedBytes;
        ub.unit = StorageSensorUnit::Bytes;
        ub.target = target;
        ub.metadata = meta;
        ub.read = [used]() { return std::optional<double>(used); };
        out.push_back(ub);

        NormalizedStorageSensor tb;
        tb.kind = StorageSensorKind::FilesystemTotalBytes;
        tb.unit = StorageSensorUnit::Bytes;
        tb.target = target;
        tb.metadata = meta;
        tb.read = [total]() { return std::optional<double>(total); };
        out.push_back(tb);
    }
    return out;
}

}  // namespace darkspark::services
