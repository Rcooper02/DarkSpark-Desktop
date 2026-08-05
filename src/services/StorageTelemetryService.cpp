// SPDX-License-Identifier: GPL-3.0-or-later
#include "services/StorageTelemetryService.hpp"

#include <QDateTime>
#include <QLoggingCategory>

#include <algorithm>

#include "services/DiskStatsProvider.hpp"
#include "services/FilesystemProvider.hpp"
#include "services/NVMeProvider.hpp"

namespace darkspark::services {

using models::MetricId;
using models::MetricSample;
using models::MetricUnit;
using models::MonotonicTimestamp;

namespace {
Q_LOGGING_CATEGORY(lcStorage, "darkspark.storage")

constexpr int kPollIntervalMs = 1000;

MonotonicTimestamp nowProd() {
    return static_cast<MonotonicTimestamp>(QDateTime::currentMSecsSinceEpoch());
}

// Human-friendly capacity for the discovery log only (diagnostic text, never UI
// telemetry). Binary units to match how drives are usually reported.
QString formatCapacity(double bytes) {
    constexpr double kTiB = 1024.0 * 1024.0 * 1024.0 * 1024.0;
    constexpr double kGiB = 1024.0 * 1024.0 * 1024.0;
    if (bytes >= kTiB) {
        return QString::number(bytes / kTiB, 'f', 1) + QStringLiteral(" TB");
    }
    return QString::number(bytes / kGiB, 'f', 1) + QStringLiteral(" GB");
}

// Collect the distinct filesystem targets (from utilization sensors) so the
// policy ranks each filesystem once.
struct FsCandidate {
    QString key;
    QString backingDevice;
    bool isRoot = false;
    bool isWritableLocal = false;
    double totalForRanking = 0.0;
};

std::vector<FsCandidate> filesystemCandidates(
    const std::vector<detail::AttributedStorageSensor>& sensors) {
    std::vector<FsCandidate> out;
    for (const auto& a : sensors) {
        if (a.sensor.kind != StorageSensorKind::FilesystemUtilization) {
            continue;
        }
        FsCandidate c;
        c.key = a.sensor.target.selectionKey;
        c.backingDevice = a.sensor.target.backingDeviceHint;
        c.isRoot = a.sensor.target.isRootFilesystem;
        c.isWritableLocal = a.sensor.target.isWritableLocal;
        c.totalForRanking = a.sensor.target.totalBytesForRanking;
        out.push_back(c);
    }
    return out;
}

}  // namespace

namespace detail {

QString selectFilesystem(const std::vector<AttributedStorageSensor>& sensors,
                         const QString& overrideKey) {
    const std::vector<FsCandidate> fs = filesystemCandidates(sensors);
    if (fs.empty()) {
        return QString();
    }

    // 1. Explicit override, if present among candidates.
    if (!overrideKey.isEmpty()) {
        for (const auto& c : fs) {
            if (c.key == overrideKey) {
                return c.key;
            }
        }
    }

    // 2. Root filesystem "/".
    for (const auto& c : fs) {
        if (c.isRoot) {
            return c.key;
        }
    }

    // 3. Largest writable local filesystem (ties broken lexicographically by key
    //    for determinism, never discovery order).
    const FsCandidate* best = nullptr;
    for (const auto& c : fs) {
        if (!c.isWritableLocal) {
            continue;
        }
        if (best == nullptr || c.totalForRanking > best->totalForRanking
            || (c.totalForRanking == best->totalForRanking
                && c.key < best->key)) {
            best = &c;
        }
    }
    if (best != nullptr) {
        return best->key;
    }

    // 4. First valid local filesystem, lexicographically by key.
    const FsCandidate* firstLocal = nullptr;
    for (const auto& c : fs) {
        if (!c.isWritableLocal && c.backingDevice.isEmpty()) {
            continue;  // skip clearly non-local
        }
        if (firstLocal == nullptr || c.key < firstLocal->key) {
            firstLocal = &c;
        }
    }
    if (firstLocal != nullptr) {
        return firstLocal->key;
    }

    // Absolute fallback: lexicographically-first key of anything present.
    QString firstKey = fs.front().key;
    for (const auto& c : fs) {
        if (c.key < firstKey) {
            firstKey = c.key;
        }
    }
    return firstKey;
}

StorageSelection selectDriveFor(
    const std::vector<AttributedStorageSensor>& sensors,
    const QString& filesystemKey, const QString& filesystemBackingDevice) {
    StorageSelection sel;
    sel.selectedFilesystemKey = filesystemKey;

    // Gather drive keys that expose a temperature sensor.
    std::vector<QString> driveKeys;
    for (const auto& a : sensors) {
        if (a.sensor.kind == StorageSensorKind::Temperature) {
            driveKeys.push_back(a.sensor.target.selectionKey);
        }
    }
    if (driveKeys.empty()) {
        return sel;  // no drive temp sources
    }

    // 1. The drive whose backing device matches the filesystem's backing device.
    //    Match if the filesystem device name CONTAINS the drive device (e.g.
    //    "/dev/nvme0n1p2" contains "nvme0n1"), the honest whole-disk relation.
    if (!filesystemBackingDevice.isEmpty()) {
        QString bestMatch;
        for (const QString& dk : driveKeys) {
            if (!dk.isEmpty() && filesystemBackingDevice.contains(dk)) {
                if (bestMatch.isEmpty() || dk.size() > bestMatch.size()
                    || (dk.size() == bestMatch.size() && dk < bestMatch)) {
                    bestMatch = dk;  // longest, then lexicographic: deterministic
                }
            }
        }
        if (!bestMatch.isEmpty()) {
            sel.selectedDriveKey = bestMatch;
            sel.driveMatchedFilesystem = true;
            return sel;
        }
    }

    // 2. Deterministic primary NVMe fallback: lexicographically-first key that
    //    looks like an nvme device.
    QString nvmeBest;
    for (const QString& dk : driveKeys) {
        if (dk.contains(QStringLiteral("nvme"))) {
            if (nvmeBest.isEmpty() || dk < nvmeBest) {
                nvmeBest = dk;
            }
        }
    }
    if (!nvmeBest.isEmpty()) {
        sel.selectedDriveKey = nvmeBest;
        return sel;
    }

    // 3. Else lexicographically-first drive key (never discovery order).
    QString firstKey = driveKeys.front();
    for (const QString& dk : driveKeys) {
        if (dk < firstKey) {
            firstKey = dk;
        }
    }
    sel.selectedDriveKey = firstKey;
    return sel;
}

DiscoverySummary summarizeDiscovery(
    const std::vector<AttributedStorageSensor>& sensors,
    const StorageSelection& selection) {
    DiscoverySummary out;
    out.selectedFilesystem = selection.selectedFilesystemKey;

    for (const auto& a : sensors) {
        const auto& tgt = a.sensor.target;
        if (tgt.selectionKey == selection.selectedFilesystemKey) {
            if (a.sensor.kind == StorageSensorKind::FilesystemUtilization) {
                out.backingDevice = tgt.backingDeviceHint;
                out.filesystemType = tgt.filesystemType;
            }
            if (a.sensor.kind == StorageSensorKind::FilesystemTotalBytes) {
                const auto v = a.sensor.read ? a.sensor.read() : std::nullopt;
                if (v.has_value()) {
                    out.capacityBytes = *v;
                }
            }
        }
        if (tgt.selectionKey == selection.selectedDriveKey) {
            if (a.sensor.kind == StorageSensorKind::Temperature) {
                out.selectedNvmeHwmon = tgt.hwmonName;
            }
            if (a.sensor.kind == StorageSensorKind::ReadRate
                || a.sensor.kind == StorageSensorKind::WriteRate) {
                out.diskStatsDevice = tgt.selectionKey;
            }
        }
    }

    // A drive was chosen by the deterministic fallback (rather than by matching
    // the filesystem's backing device) exactly when a filesystem and drive were
    // both selected but the match flag is false.
    out.usedDeterministicFallback = !selection.selectedFilesystemKey.isEmpty()
                                    && !selection.selectedDriveKey.isEmpty()
                                    && !selection.driveMatchedFilesystem;
    return out;
}

StorageTelemetryService* makeWithProviders(
    std::vector<StorageSensorProviderPtr> providers,
    std::function<MonotonicTimestamp()> now, QObject* parent) {
    return new StorageTelemetryService(std::move(providers), std::move(now),
                                       parent);
}

void pollOnceForTest(StorageTelemetryService& service) { service.poll(); }

}  // namespace detail

StorageTelemetryService::StorageTelemetryService(QObject* parent)
    : StorageTelemetryService(
          [] {
              std::vector<StorageSensorProviderPtr> p;
              p.push_back(std::make_shared<FilesystemProvider>());
              p.push_back(std::make_shared<NVMeProvider>());
              p.push_back(std::make_shared<DiskStatsProvider>());
              return p;
          }(),
          nowProd, parent) {}

StorageTelemetryService::StorageTelemetryService(
    std::vector<StorageSensorProviderPtr> providers,
    std::function<MonotonicTimestamp()> now, QObject* parent)
    : interfaces::ITelemetryProvider(parent),
      providers_(std::move(providers)),
      now_(std::move(now)) {
    timer_ = new QTimer(this);
    timer_->setInterval(kPollIntervalMs);
    connect(timer_, &QTimer::timeout, this, [this]() { poll(); });
}

void StorageTelemetryService::start() {
    if (timer_->isActive()) {
        return;
    }
    poll();
    timer_->start();
}

void StorageTelemetryService::stop() { timer_->stop(); }

void StorageTelemetryService::logDiscovery(
    const std::vector<detail::AttributedStorageSensor>& all,
    const detail::StorageSelection& sel) const {
    qCInfo(lcStorage).noquote() << "Storage Discovery:";
    QStringList seen;
    for (const auto& a : all) {
        if (!seen.contains(a.providerName)) {
            seen.append(a.providerName);
        }
    }
    for (const QString& pname : seen) {
        qCInfo(lcStorage).noquote() << "Provider:" << pname;
        for (const auto& a : all) {
            if (a.providerName == pname) {
                qCInfo(lcStorage).noquote()
                    << QStringLiteral("  \u2713 %1")
                           .arg(a.sensor.metadata.stableId);
            }
        }
    }

    // Resolve the selected identity through the same summary the tests assert
    // on, so the log and the tested contract can never drift apart.
    const detail::DiscoverySummary sum = detail::summarizeDiscovery(all, sel);

    qCInfo(lcStorage).noquote()
        << "Selected filesystem:"
        << (sum.selectedFilesystem.isEmpty() ? QStringLiteral("(none)")
                                             : sum.selectedFilesystem);
    if (!sum.selectedFilesystem.isEmpty()) {
        qCInfo(lcStorage).noquote()
            << "Backing device:"
            << (sum.backingDevice.isEmpty() ? QStringLiteral("(undetermined)")
                                            : sum.backingDevice);
        qCInfo(lcStorage).noquote()
            << "Filesystem type:" << sum.filesystemType;
        qCInfo(lcStorage).noquote()
            << "Capacity:" << formatCapacity(sum.capacityBytes);
    }

    if (!sum.selectedNvmeHwmon.isEmpty()) {
        qCInfo(lcStorage).noquote()
            << "Selected NVMe hwmon:" << sum.selectedNvmeHwmon;
    }
    if (!sum.diskStatsDevice.isEmpty()) {
        qCInfo(lcStorage).noquote()
            << "DiskStats device:" << sum.diskStatsDevice;
    }

    // No silent fallback: state explicitly when the drive was chosen because the
    // filesystem's backing device could not be tied to a drive.
    if (sum.usedDeterministicFallback) {
        qCInfo(lcStorage).noquote()
            << "Filesystem backing device could not be determined.";
        qCInfo(lcStorage).noquote()
            << "Falling back to deterministic primary NVMe.";
    }
}

void StorageTelemetryService::emitMetric(
    MetricId id, MetricUnit unit,
    const std::optional<detail::AttributedStorageSensor>& sensor,
    std::optional<MetricSample>& lastSample, std::optional<double>& lastValue) {
    const MonotonicTimestamp t = now_ ? now_() : 0;
    if (!sensor.has_value()) {
        const auto s = MetricSample::unavailable(id, t);
        lastSample = s;
        emit readingChanged(s);
        return;
    }
    const std::string key = sensor->sensor.metadata.stableId.toStdString();
    const std::optional<double> value =
        sensor->sensor.read ? sensor->sensor.read() : std::nullopt;
    if (value.has_value()) {
        lastValue = value;
        if (const auto s = MetricSample::tryFresh(id, *value, unit, t, key)) {
            lastSample = s;
            emit readingChanged(*s);
        }
        return;
    }
    if (lastValue.has_value()) {
        if (const auto s =
                MetricSample::tryStale(id, *lastValue, unit, t, key)) {
            lastSample = s;
            emit readingChanged(*s);
        }
    } else {
        const auto s = MetricSample::unavailable(id, t, key);
        lastSample = s;
        emit readingChanged(s);
    }
}

void StorageTelemetryService::poll() {
    std::vector<detail::AttributedStorageSensor> all;
    for (const auto& p : providers_) {
        if (!p) {
            continue;
        }
        const QString pname = p->providerName();
        for (auto& s : p->discover()) {
            all.push_back(detail::AttributedStorageSensor{pname, std::move(s)});
        }
    }

    // Filesystem selection, then disk selection relative to it.
    const QString fsKey = detail::selectFilesystem(all, overrideFilesystemKey_);
    QString fsBacking;
    for (const auto& a : all) {
        if (a.sensor.kind == StorageSensorKind::FilesystemUtilization
            && a.sensor.target.selectionKey == fsKey) {
            fsBacking = a.sensor.target.backingDeviceHint;
            break;
        }
    }
    const detail::StorageSelection sel =
        detail::selectDriveFor(all, fsKey, fsBacking);

    if (!loggedDiscovery_) {
        logDiscovery(all, sel);
        loggedDiscovery_ = true;
    }

    // Resolve the selected sensors by (kind, selectionKey).
    auto find = [&](StorageSensorKind kind, const QString& key)
        -> std::optional<detail::AttributedStorageSensor> {
        for (const auto& a : all) {
            if (a.sensor.kind == kind
                && a.sensor.target.selectionKey == key) {
                return a;
            }
        }
        return std::nullopt;
    };

    emitMetric(MetricId::StorageUtilization, MetricUnit::Percent,
               find(StorageSensorKind::FilesystemUtilization, fsKey),
               utilization_, lastUtil_);
    emitMetric(MetricId::StorageUsedBytes, MetricUnit::Bytes,
               find(StorageSensorKind::FilesystemUsedBytes, fsKey), usedBytes_,
               lastUsed_);
    emitMetric(MetricId::StorageTotalBytes, MetricUnit::Bytes,
               find(StorageSensorKind::FilesystemTotalBytes, fsKey), totalBytes_,
               lastTotal_);
    emitMetric(MetricId::StorageTemperature, MetricUnit::Celsius,
               find(StorageSensorKind::Temperature, sel.selectedDriveKey),
               temperature_, lastTemp_);
    emitMetric(MetricId::StorageReadRate, MetricUnit::BytesPerSecond,
               find(StorageSensorKind::ReadRate, sel.selectedDriveKey),
               readRate_, lastRead_);
    emitMetric(MetricId::StorageWriteRate, MetricUnit::BytesPerSecond,
               find(StorageSensorKind::WriteRate, sel.selectedDriveKey),
               writeRate_, lastWrite_);
}

QList<models::MetricSample> StorageTelemetryService::currentSamples() const {
    QList<models::MetricSample> out;
    auto add = [&](const std::optional<MetricSample>& s, MetricId id) {
        out.append(s.value_or(MetricSample::unavailable(id, 0)));
    };
    add(utilization_, MetricId::StorageUtilization);
    add(usedBytes_, MetricId::StorageUsedBytes);
    add(totalBytes_, MetricId::StorageTotalBytes);
    add(temperature_, MetricId::StorageTemperature);
    add(readRate_, MetricId::StorageReadRate);
    add(writeRate_, MetricId::StorageWriteRate);
    return out;
}

}  // namespace darkspark::services
