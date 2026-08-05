// SPDX-License-Identifier: GPL-3.0-or-later
#include "services/NVMeProvider.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace darkspark::services {

namespace {

QString readFileTrimmed(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }
    return QString::fromUtf8(f.readAll()).trimmed();
}

std::optional<double> readCelsius(const QString& path) {
    const QString raw = readFileTrimmed(path);
    if (raw.isEmpty()) {
        return std::nullopt;
    }
    bool ok = false;
    const double milli = raw.toDouble(&ok);
    if (!ok) {
        return std::nullopt;
    }
    return milli / 1000.0;
}

// Try to resolve the block device backing an nvme hwmon directory. hwmon for an
// nvme device typically lives at /sys/class/hwmon/hwmonN -> ../../nvme/nvmeX,
// and the block device is nvmeXn1. We resolve via the canonical device path.
// HONEST LIMIT: if the structure differs, we return empty rather than guess.
QString resolveNvmeBlockDevice(const QString& hwmonDir) {
    // hwmonDir/device is a symlink toward the nvme controller.
    const QString devLink = hwmonDir + QStringLiteral("/device");
    const QString canon = QFileInfo(devLink).canonicalFilePath();
    if (canon.isEmpty()) {
        return QString();
    }
    // Look for an "nvmeXnY" block node under the controller directory.
    QDir controller(canon);
    const QStringList blocks = controller.entryList(
        QStringList{QStringLiteral("nvme*n*")}, QDir::Dirs, QDir::Name);
    if (!blocks.isEmpty()) {
        return blocks.first();  // deterministic: entryList sorted by name
    }
    // Some layouts expose the namespace one level up.
    const QString base = QFileInfo(canon).fileName();  // e.g. "nvme0"
    if (base.startsWith(QStringLiteral("nvme"))) {
        return base + QStringLiteral("n1");  // conventional first namespace
    }
    return QString();
}

std::vector<DriveTemperatureSource> enumerateProd() {
    std::vector<DriveTemperatureSource> out;
    QDir hwmonRoot(QStringLiteral("/sys/class/hwmon"));
    const QStringList entries = hwmonRoot.entryList(
        QStringList{QStringLiteral("hwmon*")}, QDir::Dirs, QDir::Name);
    for (const QString& e : entries) {
        const QString dir = hwmonRoot.absoluteFilePath(e);
        const QString name = readFileTrimmed(dir + QStringLiteral("/name"));
        if (name != QStringLiteral("nvme")) {
            continue;  // only NVMe drives in V1
        }
        QDir d(dir);
        const QStringList temps = d.entryList(
            QStringList{QStringLiteral("temp*_input")}, QDir::Files, QDir::Name);
        if (temps.isEmpty()) {
            continue;
        }
        // Prefer "Composite" label if present; else temp1_input (deterministic).
        QString chosen = temps.first();
        for (const QString& t : temps) {
            QString base = t;
            base.chop(static_cast<int>(QStringLiteral("_input").size()));
            const QString label =
                readFileTrimmed(d.absoluteFilePath(base + QStringLiteral("_label")));
            if (label.compare(QStringLiteral("Composite"), Qt::CaseInsensitive)
                == 0) {
                chosen = t;
                break;
            }
        }
        DriveTemperatureSource src;
        src.isNvme = true;
        src.inputPath = d.absoluteFilePath(chosen);
        src.hwmonName = e;
        src.blockDevice = resolveNvmeBlockDevice(dir);
        src.model = readFileTrimmed(dir + QStringLiteral("/device/model"));
        const QString idBase =
            src.blockDevice.isEmpty() ? e : src.blockDevice;
        src.stableId = QStringLiteral("nvme:") + idBase;
        out.push_back(src);
    }
    return out;
}

}  // namespace

NVMeProvider::NVMeProvider() : enumerator_(enumerateProd) {}

NVMeProvider::NVMeProvider(Enumerator enumerator)
    : enumerator_(std::move(enumerator)) {}

QString NVMeProvider::providerName() const {
    return QStringLiteral("nvme");
}

std::vector<NormalizedStorageSensor> NVMeProvider::discover() const {
    std::vector<NormalizedStorageSensor> out;
    const std::vector<DriveTemperatureSource> drives =
        enumerator_ ? enumerator_() : std::vector<DriveTemperatureSource>{};
    for (const DriveTemperatureSource& drv : drives) {
        StorageTarget target;
        // The selection key for a drive is its block device (or stableId when
        // the block device is unknown), so the disk-selection policy can match
        // it against a filesystem's backing device.
        target.selectionKey =
            drv.blockDevice.isEmpty() ? drv.stableId : drv.blockDevice;
        target.backingDeviceHint = drv.blockDevice;
        target.hwmonName = drv.hwmonName;

        SensorMetadata meta;
        meta.model = drv.model;
        meta.label = drv.blockDevice.isEmpty() ? drv.stableId : drv.blockDevice;
        meta.stableId = drv.stableId;
        meta.devicePath = drv.inputPath;
        meta.capabilities = QStringList{QStringLiteral("temperature")};

        const QString path = drv.inputPath;
        NormalizedStorageSensor t;
        t.kind = StorageSensorKind::Temperature;
        t.unit = StorageSensorUnit::Celsius;
        t.target = target;
        t.metadata = meta;
        t.read = [path]() { return readCelsius(path); };
        out.push_back(t);
    }
    return out;
}

}  // namespace darkspark::services
