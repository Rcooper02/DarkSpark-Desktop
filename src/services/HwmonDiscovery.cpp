// SPDX-License-Identifier: GPL-3.0-or-later
#include "services/HwmonDiscovery.hpp"

#include <algorithm>
#include <optional>

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileInfoList>
#include <QRegularExpression>
#include <QRegularExpressionMatch>

#include "models/MetricSample.hpp"

namespace darkspark::services {

using models::MetricId;
using models::MetricUnit;
using models::SensorDefinition;
using models::SensorKey;

namespace {

/// hwmon device name that exposes AMD (Zen) CPU temperatures. Only this device
/// is selected in this batch.
constexpr char kK10Temp[] = "k10temp";

/// Read the first line of a small sysfs-style file, trimmed. Returns nullopt if
/// the file cannot be opened. Never throws.
[[nodiscard]] std::optional<QString> readTrimmed(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return std::nullopt;
    }
    const QByteArray raw = file.readAll();
    return QString::fromUtf8(raw).trimmed();
}

/// Map an hwmon temperature label to a stable, lowercase logical sensor key.
/// Returns nullopt for labels this batch does not handle.
///
///   Tctl, Tdie -> "package"   (both denote the package-level figure; if both
///                              are present they resolve to the same key and are
///                              de-duplicated by the caller)
///   TccdN      -> "ccdN"       (lowercase, N preserved)
[[nodiscard]] std::optional<QString> logicalKeyForLabel(const QString& label) {
    const QString trimmed = label.trimmed();
    if (trimmed.compare(QStringLiteral("Tctl"), Qt::CaseInsensitive) == 0
        || trimmed.compare(QStringLiteral("Tdie"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("package");
    }
    // Tccd<N> -> ccd<N>
    static const QRegularExpression ccd(
        QStringLiteral("^Tccd(\\d+)$"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch m = ccd.match(trimmed);
    if (m.hasMatch()) {
        return QStringLiteral("ccd") + m.captured(1);
    }
    return std::nullopt;
}

/// Human-facing display name for a logical key.
[[nodiscard]] QString displayNameForKey(const QString& key) {
    if (key == QStringLiteral("package")) {
        return QStringLiteral("CPU Package");
    }
    if (key.startsWith(QStringLiteral("ccd"))) {
        // ccd1 -> "CPU CCD1"
        return QStringLiteral("CPU CCD") + key.mid(3);
    }
    return QStringLiteral("CPU ") + key;
}

/// From a temp*_label filename, derive the paired temp*_input filename.
/// e.g. "temp1_label" -> "temp1_input". Returns nullopt if the name does not
/// match the expected pattern.
[[nodiscard]] std::optional<QString> pairedInputName(const QString& labelName) {
    static const QRegularExpression re(QStringLiteral("^(temp\\d+)_label$"));
    const QRegularExpressionMatch m = re.match(labelName);
    if (!m.hasMatch()) {
        return std::nullopt;
    }
    return m.captured(1) + QStringLiteral("_input");
}

}  // namespace

HwmonDiscovery::HwmonDiscovery(QString root) : root_(std::move(root)) {}

QString HwmonDiscovery::defaultRoot() {
    return QStringLiteral("/sys/class/hwmon");
}

QList<DiscoveredSensor> HwmonDiscovery::discover() const {
    QList<DiscoveredSensor> results;
    // Track keys already bound so duplicate logical labels (e.g. Tctl and Tdie
    // both present, or two Tccd1 entries) resolve deterministically to the first
    // successful binding.
    QList<QString> boundKeys;

    QDir root(root_);
    if (!root.exists()) {
        return results;
    }

    // Enumerate hwmon* directories. Numbering is not assumed stable, so we take
    // whatever exists and re-resolve every call.
    const QFileInfoList devices =
        root.entryInfoList({QStringLiteral("hwmon*")},
                           QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);

    for (const QFileInfo& deviceInfo : devices) {
        const QString devicePath = deviceInfo.absoluteFilePath();

        // Identify the device by its `name` file, never by its hwmonN index.
        const std::optional<QString> name =
            readTrimmed(devicePath + QStringLiteral("/name"));
        if (!name.has_value()) {
            continue;  // missing name file -> unidentifiable -> skip
        }
        if (name.value() != QString::fromLatin1(kK10Temp)) {
            continue;  // unrelated device -> skip
        }

        // Enumerate temp*_label files within this device.
        QDir deviceDir(devicePath);
        const QFileInfoList labels =
            deviceDir.entryInfoList({QStringLiteral("temp*_label")},
                                    QDir::Files, QDir::Name);

        for (const QFileInfo& labelInfo : labels) {
            const std::optional<QString> labelText =
                readTrimmed(labelInfo.absoluteFilePath());
            if (!labelText.has_value()) {
                continue;  // label file unreadable -> skip
            }

            const std::optional<QString> key =
                logicalKeyForLabel(labelText.value());
            if (!key.has_value()) {
                continue;  // label we do not map in this batch -> skip
            }
            if (boundKeys.contains(key.value())) {
                continue;  // duplicate logical key -> keep first, skip rest
            }

            const std::optional<QString> inputName =
                pairedInputName(labelInfo.fileName());
            if (!inputName.has_value()) {
                continue;  // label filename not in temp<N>_label form -> skip
            }
            const QString inputPath =
                devicePath + QStringLiteral("/") + inputName.value();

            // The paired input must exist and be readable now; a label without a
            // usable input is not a bindable sensor.
            const std::optional<QString> inputRaw = readTrimmed(inputPath);
            if (!inputRaw.has_value()) {
                continue;  // missing paired input -> skip
            }
            bool ok = false;
            (void)inputRaw.value().toLongLong(&ok);
            if (!ok) {
                continue;  // malformed numeric value -> skip
            }

            SensorDefinition definition;
            definition.identity =
                SensorKey{MetricId::CpuTemperature, key.value().toStdString()};
            definition.displayName =
                displayNameForKey(key.value()).toStdString();
            definition.unit = MetricUnit::Celsius;

            DiscoveredSensor sensor;
            sensor.definition = definition;
            sensor.inputPath = inputPath;
            results.append(sensor);
            boundKeys.append(key.value());
        }
    }

    // Deterministic order by sensor key, independent of directory iteration
    // order, so callers and tests see a stable sequence.
    std::sort(results.begin(), results.end(),
              [](const DiscoveredSensor& a, const DiscoveredSensor& b) {
                  return a.definition.identity.key < b.definition.identity.key;
              });
    return results;
}

}  // namespace darkspark::services
