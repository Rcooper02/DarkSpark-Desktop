// SPDX-License-Identifier: GPL-3.0-or-later
#include "services/DiskStatsProvider.hpp"

#include <QDateTime>
#include <QFile>

namespace darkspark::services {

namespace {

constexpr std::uint64_t kSectorBytes = 512;

// Whether a diskstats device name is a whole physical disk we care about (not a
// partition or a virtual device). V1: nvme whole devices (nvmeXnY) and sdX.
bool isWholeDisk(const QString& dev) {
    if (dev.startsWith(QStringLiteral("nvme"))) {
        // nvme0n1 yes; nvme0n1p2 (partition) no.
        return !dev.contains(QStringLiteral("p"));
    }
    if (dev.startsWith(QStringLiteral("sd")) && dev.size() == 3) {
        return true;  // sda, sdb; not sda1
    }
    return false;
}

std::vector<DiskCounters> readProcDiskstats() {
    std::vector<DiskCounters> out;
    QFile f(QStringLiteral("/proc/diskstats"));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return out;
    }
    const QString content = QString::fromUtf8(f.readAll());
    const QStringList lines =
        content.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString& line : lines) {
        const QStringList f2 =
            line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        // Fields: 0 major 1 minor 2 name 3 reads 4 rd_merged 5 sectors_read
        //         6 rd_ms 7 writes 8 wr_merged 9 sectors_written ...
        if (f2.size() < 10) {
            continue;
        }
        const QString dev = f2.at(2);
        if (!isWholeDisk(dev)) {
            continue;
        }
        bool okR = false;
        bool okW = false;
        const quint64 sectorsRead = f2.at(5).toULongLong(&okR);
        const quint64 sectorsWritten = f2.at(9).toULongLong(&okW);
        if (!okR || !okW) {
            continue;
        }
        DiskCounters c;
        c.device = dev;
        c.bytesRead = static_cast<std::uint64_t>(sectorsRead) * kSectorBytes;
        c.bytesWritten =
            static_cast<std::uint64_t>(sectorsWritten) * kSectorBytes;
        out.push_back(c);
    }
    return out;
}

std::int64_t nowMsProd() {
    return static_cast<std::int64_t>(QDateTime::currentMSecsSinceEpoch());
}

}  // namespace

DiskStatsProvider::DiskStatsProvider()
    : counters_(readProcDiskstats), clock_(nowMsProd) {}

DiskStatsProvider::DiskStatsProvider(CounterSource counters, Clock clock)
    : counters_(std::move(counters)), clock_(std::move(clock)) {}

QString DiskStatsProvider::providerName() const {
    return QStringLiteral("diskstats");
}

std::vector<NormalizedStorageSensor> DiskStatsProvider::discover() const {
    std::vector<NormalizedStorageSensor> out;
    const std::vector<DiskCounters> counters =
        counters_ ? counters_() : std::vector<DiskCounters>{};
    const std::int64_t nowMs = clock_ ? clock_() : 0;

    for (const DiskCounters& c : counters) {
        const std::string key = c.device.toStdString();
        DeviceRates& rates = state_[key];
        // Advance the rate calculators now; capture the computed rate (or
        // nullopt) so the read() closure returns a stable, already-decided
        // value for this poll. The provider owns all rate state.
        rates.lastReadRate = rates.read.update(c.bytesRead, nowMs);
        rates.lastWriteRate = rates.write.update(c.bytesWritten, nowMs);

        StorageTarget target;
        target.selectionKey = c.device;
        target.backingDeviceHint = c.device;

        SensorMetadata meta;
        meta.label = c.device;
        meta.stableId = QStringLiteral("disk:") + c.device;
        meta.devicePath = QStringLiteral("/dev/") + c.device;
        meta.capabilities =
            QStringList{QStringLiteral("read"), QStringLiteral("write")};

        const std::optional<double> readRate = rates.lastReadRate;
        NormalizedStorageSensor r;
        r.kind = StorageSensorKind::ReadRate;
        r.unit = StorageSensorUnit::BytesPerSecond;
        r.target = target;
        r.metadata = meta;
        r.read = [readRate]() { return readRate; };
        out.push_back(r);

        const std::optional<double> writeRate = rates.lastWriteRate;
        NormalizedStorageSensor w;
        w.kind = StorageSensorKind::WriteRate;
        w.unit = StorageSensorUnit::BytesPerSecond;
        w.target = target;
        w.metadata = meta;
        w.read = [writeRate]() { return writeRate; };
        out.push_back(w);
    }
    return out;
}

}  // namespace darkspark::services
