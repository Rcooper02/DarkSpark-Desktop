// SPDX-License-Identifier: GPL-3.0-or-later
#include "deck/layout/LayoutPersistenceService.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QStandardPaths>

#include "deck/layout/DeckLayoutSerializer.hpp"

namespace darkspark::deck::layout {

namespace {

Q_LOGGING_CATEGORY(lcLayoutPersist, "darkspark.layout.persistence")

constexpr const char* kLayoutFileName = "command-deck-layout.json";

/// Resolve the production path under QStandardPaths::AppConfigLocation. With
/// org "DarkSpark" and app "DarkSpark Desktop" this is
/// ~/.config/DarkSpark/DarkSpark Desktop/command-deck-layout.json on Linux.
QString resolveProductionPath() {
    const QString dir =
        QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    return QDir(dir).filePath(QLatin1String(kLayoutFileName));
}

}  // namespace

LayoutPersistenceService::LayoutPersistenceService()
    : filePath_(resolveProductionPath()) {}

LayoutPersistenceService::LayoutPersistenceService(QString filePath)
    : filePath_(std::move(filePath)) {}

bool LayoutPersistenceService::save(const DeckLayout& layout) {
    const QFileInfo info(filePath_);
    const QDir dir = info.dir();
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        qCWarning(lcLayoutPersist)
            << "could not create config directory" << dir.absolutePath();
        return false;
    }
    QFile f(filePath_);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        qCWarning(lcLayoutPersist)
            << "could not open layout file for writing" << filePath_;
        return false;
    }
    const QByteArray bytes = serializeLayout(layout);
    const qint64 written = f.write(bytes);
    f.close();
    if (written != bytes.size()) {
        qCWarning(lcLayoutPersist) << "short write to layout file" << filePath_;
        return false;
    }
    return true;
}

DeckLayout LayoutPersistenceService::loadOrDefault() {
    const DeckLayout& def = defaultCommandDeckLayout();

    QFile f(filePath_);
    if (!f.exists()) {
        // First run: materialise the compiled default so a real, human-readable
        // file exists from day one.
        qCInfo(lcLayoutPersist)
            << "no layout file at" << filePath_ << "-- writing compiled default";
        save(def);
        return def;
    }

    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCWarning(lcLayoutPersist)
            << "layout file unreadable" << filePath_
            << "-- recovering with compiled default";
        save(def);
        return def;
    }
    const QByteArray bytes = f.readAll();
    f.close();

    const std::optional<DeckLayout> parsed = deserializeLayout(bytes);
    if (!parsed) {
        qCWarning(lcLayoutPersist)
            << "layout file malformed or unknown version" << filePath_
            << "-- restoring compiled default and rewriting";
        save(def);
        return def;
    }
    if (!isValidLayout(*parsed)) {
        qCWarning(lcLayoutPersist)
            << "layout file parsed but failed validation" << filePath_
            << "-- restoring compiled default and rewriting";
        save(def);
        return def;
    }

    qCInfo(lcLayoutPersist) << "loaded persisted layout from" << filePath_;
    return *parsed;
}

}  // namespace darkspark::deck::layout
