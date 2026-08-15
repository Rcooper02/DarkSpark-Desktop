// SPDX-License-Identifier: GPL-3.0-or-later
#include "deck/layout/LayoutPersistenceService.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLoggingCategory>
#include <QStandardPaths>

#include <optional>

#include "deck/layout/DeckLayoutSerializer.hpp"

namespace darkspark::deck::layout {

namespace {

Q_LOGGING_CATEGORY(lcLayoutPersist, "darkspark.layout.persistence")

constexpr const char* kLayoutFileName = "command-deck-layout.json";

/// Read only the top-level "version" integer from JSON bytes, without fully
/// parsing the layout. Returns nullopt when the bytes are not a JSON object or
/// have no integer version. Used to distinguish a legacy v1 file (which
/// deserializeCollection migrates) from a v2 file, so we rewrite only after a
/// real migration.
std::optional<int> rawSchemaVersion(const QByteArray& bytes) {
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(bytes, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return std::nullopt;
    }
    const QJsonValue version = doc.object().value(QStringLiteral("version"));
    if (!version.isDouble()) {
        return std::nullopt;
    }
    return version.toInt();
}

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

bool LayoutPersistenceService::writeBytes(const QByteArray& bytes) {
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
    const qint64 written = f.write(bytes);
    f.close();
    if (written != bytes.size()) {
        qCWarning(lcLayoutPersist) << "short write to layout file" << filePath_;
        return false;
    }
    return true;
}

bool LayoutPersistenceService::save(const DeckLayout& layout) {
    return writeBytes(serializeLayout(layout));
}

bool LayoutPersistenceService::saveCollection(
    const DeckLayoutCollection& collection) {
    return writeBytes(serializeCollection(collection));
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

DeckLayoutCollection LayoutPersistenceService::loadCollectionOrDefault() {
    const DeckLayoutCollection& def = defaultCommandDeckCollection();

    QFile f(filePath_);
    if (!f.exists()) {
        qCInfo(lcLayoutPersist)
            << "no layout file at" << filePath_
            << "-- writing compiled 3-page default";
        saveCollection(def);
        return def;
    }

    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCWarning(lcLayoutPersist)
            << "layout file unreadable" << filePath_
            << "-- recovering with compiled default";
        saveCollection(def);
        return def;
    }
    const QByteArray bytes = f.readAll();
    f.close();

    // deserializeCollection handles v2 directly and migrates v1 in-place; an
    // unknown/newer version or malformed bytes yield nullopt -> default.
    const std::optional<DeckLayoutCollection> parsed =
        deserializeCollection(bytes);
    if (!parsed) {
        qCWarning(lcLayoutPersist)
            << "layout file malformed or unknown version" << filePath_
            << "-- restoring compiled 3-page default and rewriting";
        saveCollection(def);
        return def;
    }
    if (!isValidCollection(*parsed)) {
        qCWarning(lcLayoutPersist)
            << "layout collection parsed but failed validation" << filePath_
            << "-- restoring compiled default and rewriting";
        saveCollection(def);
        return def;
    }

    // If the file was a legacy v1 layout, deserializeCollection returned a
    // migrated collection; rewrite the file as v2 so migration is one-way and
    // persistent. Detect legacy by reading the raw top-level version field
    // rather than inspecting formatted bytes.
    if (rawSchemaVersion(bytes).value_or(0) == kLegacyLayoutSchemaVersion) {
        qCInfo(lcLayoutPersist)
            << "migrated v1 layout to v2 collection; rewriting" << filePath_;
        saveCollection(*parsed);
    } else {
        qCInfo(lcLayoutPersist)
            << "loaded persisted v2 collection from" << filePath_;
    }
    return *parsed;
}

bool LayoutPersistenceService::saveActivePage(int activePageId) {
    // Re-read the current collection (or default), update only the active page,
    // and write it back. Called on real page switches, not per frame.
    DeckLayoutCollection collection = loadCollectionOrDefault();
    bool exists = false;
    for (const DeckPageDefinition& page : collection.pages) {
        if (page.pageId == activePageId) {
            exists = true;
            break;
        }
    }
    if (!exists) {
        qCWarning(lcLayoutPersist)
            << "refusing to persist unknown activePageId" << activePageId;
        return false;
    }
    if (collection.activePageId == activePageId) {
        return true;  // no change; avoid an unnecessary rewrite
    }
    collection.activePageId = activePageId;
    return saveCollection(collection);
}

}  // namespace darkspark::deck::layout
