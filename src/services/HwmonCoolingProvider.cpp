// SPDX-License-Identifier: GPL-3.0-or-later
#include "services/HwmonCoolingProvider.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include "services/GpuDeviceSelector.hpp"

namespace darkspark::services {

namespace {

constexpr double kImplausiblyHighRpm = 60000.0;

QString readFileTrimmed(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }
    return QString::fromUtf8(f.readAll()).trimmed();
}

std::optional<double> readRpmFile(const QString& path) {
    const QString raw = readFileTrimmed(path);
    if (raw.isEmpty()) {
        return std::nullopt;
    }
    bool ok = false;
    const double v = raw.toDouble(&ok);
    if (!ok || v < 0.0 || v > kImplausiblyHighRpm) {
        return std::nullopt;
    }
    return v;
}

std::optional<double> readCelsiusFile(const QString& path) {
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

CoolingSensorRole classifyFan(const QString& hwmonName, const QString& label,
                              bool onGpuHwmon) {
    const QString l = label.toLower();
    if (l.contains(QStringLiteral("pump"))) {
        return CoolingSensorRole::PumpRpm;
    }
    if (onGpuHwmon) {
        return CoolingSensorRole::GpuFanRpm;
    }
    if (l.contains(QStringLiteral("cpu"))) {
        return CoolingSensorRole::CpuFanRpm;
    }
    const QString n = hwmonName.toLower();
    if (n.startsWith(QStringLiteral("nct")) || n.startsWith(QStringLiteral("it"))) {
        return CoolingSensorRole::CpuFanRpm;
    }
    return CoolingSensorRole::CaseFanRpm;
}

}  // namespace

HwmonCoolingProvider::HwmonCoolingProvider(QString root)
    : root_(std::move(root)) {}

QString HwmonCoolingProvider::defaultRoot() {
    return QStringLiteral("/sys/class/hwmon");
}

QString HwmonCoolingProvider::providerName() const {
    return QStringLiteral("hwmon");
}

QString HwmonCoolingProvider::gpuHwmonPath() const {
    if (gpuResolved_) {
        return gpuHwmonPath_;
    }
    // Reuse the shared GPU selection so a GPU fan binds to the same physical GPU
    // the rest of the app uses. Cached for the lifetime of this provider.
    auto* self = const_cast<HwmonCoolingProvider*>(this);
    if (self->gpuHwmonPath_.isEmpty()) {
        if (const auto sel = selectGpuDevice(); sel && sel->hasHwmon) {
            self->gpuHwmonPath_ = sel->hwmonPath;
        }
    }
    self->gpuResolved_ = true;
    return gpuHwmonPath_;
}

std::vector<NormalizedCoolingSensor> HwmonCoolingProvider::discover() const {
    std::vector<NormalizedCoolingSensor> out;
    const QString gpuHwmon = gpuHwmonPath();

    QDir hwmonRoot(root_);
    const QStringList entries = hwmonRoot.entryList(
        QStringList{QStringLiteral("hwmon*")}, QDir::Dirs, QDir::Name);
    for (const QString& e : entries) {
        const QString dir = hwmonRoot.absoluteFilePath(e);
        const QString name = readFileTrimmed(dir + QStringLiteral("/name"));
        if (name.isEmpty()) {
            continue;
        }
        const bool onGpuHwmon =
            !gpuHwmon.isEmpty()
            && QFileInfo(dir).canonicalFilePath()
                   == QFileInfo(gpuHwmon).canonicalFilePath();
        QDir d(dir);

        // Fan RPM inputs.
        const QStringList fans = d.entryList(
            QStringList{QStringLiteral("fan*_input")}, QDir::Files, QDir::Name);
        for (const QString& fan : fans) {
            const QString inputPath = d.absoluteFilePath(fan);
            QString base = fan;
            base.chop(static_cast<int>(QStringLiteral("_input").size()));
            const QString label = readFileTrimmed(
                d.absoluteFilePath(base + QStringLiteral("_label")));

            NormalizedCoolingSensor s;
            s.role = classifyFan(name, label, onGpuHwmon);
            s.unit = CoolingSensorUnit::Rpm;
            s.metadata.label =
                label.isEmpty() ? base : label;
            s.metadata.stableId = name + QLatin1Char(':') + base;
            s.metadata.devicePath = inputPath;
            s.metadata.capabilities = QStringList{QStringLiteral("rpm")};
            s.read = [inputPath]() { return readRpmFile(inputPath); };
            out.push_back(std::move(s));
        }

        // Coolant temperature inputs (label contains coolant/water).
        const QStringList temps = d.entryList(
            QStringList{QStringLiteral("temp*_label")}, QDir::Files, QDir::Name);
        for (const QString& tl : temps) {
            const QString label = readFileTrimmed(d.absoluteFilePath(tl));
            const QString ll = label.toLower();
            if (!ll.contains(QStringLiteral("coolant"))
                && !ll.contains(QStringLiteral("water"))) {
                continue;
            }
            QString base = tl;
            base.chop(static_cast<int>(QStringLiteral("_label").size()));
            const QString inputPath =
                d.absoluteFilePath(base + QStringLiteral("_input"));

            NormalizedCoolingSensor s;
            s.role = CoolingSensorRole::CoolantTemp;
            s.unit = CoolingSensorUnit::Celsius;
            s.metadata.label = label;
            s.metadata.stableId = name + QLatin1Char(':') + base;
            s.metadata.devicePath = inputPath;
            s.metadata.capabilities = QStringList{QStringLiteral("temp")};
            s.read = [inputPath]() { return readCelsiusFile(inputPath); };
            out.push_back(std::move(s));
        }
    }
    return out;
}

}  // namespace darkspark::services
