// SPDX-License-Identifier: GPL-3.0-or-later
#include "deck/layout/DeckLayoutSerializer.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

namespace darkspark::deck::layout {

// --- enum <-> string mappings ------------------------------------------------

QString widgetIdToString(WidgetId id) {
    switch (id) {
    case WidgetId::Cpu:
        return QStringLiteral("cpu");
    case WidgetId::Gpu:
        return QStringLiteral("gpu");
    case WidgetId::Memory:
        return QStringLiteral("memory");
    case WidgetId::Cooling:
        return QStringLiteral("cooling");
    case WidgetId::Storage:
        return QStringLiteral("storage");
    case WidgetId::Network:
        return QStringLiteral("network");
    case WidgetId::Unknown:
        return QString();  // sentinel: never serialised
    }
    return QString();
}

std::optional<WidgetId> widgetIdFromString(const QString& s) {
    if (s == QLatin1String("cpu")) return WidgetId::Cpu;
    if (s == QLatin1String("gpu")) return WidgetId::Gpu;
    if (s == QLatin1String("memory")) return WidgetId::Memory;
    if (s == QLatin1String("cooling")) return WidgetId::Cooling;
    if (s == QLatin1String("storage")) return WidgetId::Storage;
    if (s == QLatin1String("network")) return WidgetId::Network;
    return std::nullopt;  // unknown token -> graceful fallback
}

QString regionToString(DeckRegion region) {
    switch (region) {
    case DeckRegion::Primary:
        return QStringLiteral("primary");
    case DeckRegion::Secondary:
        return QStringLiteral("secondary");
    }
    return QString();
}

std::optional<DeckRegion> regionFromString(const QString& s) {
    if (s == QLatin1String("primary")) return DeckRegion::Primary;
    if (s == QLatin1String("secondary")) return DeckRegion::Secondary;
    return std::nullopt;
}

QString sizeModeToString(InstrumentSizeMode mode) {
    switch (mode) {
    case InstrumentSizeMode::Small:
        return QStringLiteral("small");
    case InstrumentSizeMode::Medium:
        return QStringLiteral("medium");
    case InstrumentSizeMode::Large:
        return QStringLiteral("large");
    case InstrumentSizeMode::Wide:
        return QStringLiteral("wide");
    }
    return QString();
}

std::optional<InstrumentSizeMode> sizeModeFromString(const QString& s) {
    if (s == QLatin1String("small")) return InstrumentSizeMode::Small;
    if (s == QLatin1String("medium")) return InstrumentSizeMode::Medium;
    if (s == QLatin1String("large")) return InstrumentSizeMode::Large;
    if (s == QLatin1String("wide")) return InstrumentSizeMode::Wide;
    return std::nullopt;
}

// --- DeckLayout <-> JSON -----------------------------------------------------

namespace {

QJsonObject placementToJson(const DeckWidgetPlacement& p) {
    QJsonObject o;
    o.insert(QStringLiteral("widgetId"), widgetIdToString(p.id));
    o.insert(QStringLiteral("region"), regionToString(p.region));
    o.insert(QStringLiteral("row"), p.row);
    o.insert(QStringLiteral("column"), p.column);
    o.insert(QStringLiteral("rowSpan"), p.rowSpan);
    o.insert(QStringLiteral("columnSpan"), p.columnSpan);
    o.insert(QStringLiteral("sizeMode"), sizeModeToString(p.sizeMode));
    o.insert(QStringLiteral("enabled"), p.enabled);
    return o;
}

/// Parse one placement object. nullopt when a field is missing, the wrong JSON
/// type, or an enum string is unrecognised.
std::optional<DeckWidgetPlacement> placementFromJson(const QJsonObject& o) {
    const QJsonValue widgetId = o.value(QStringLiteral("widgetId"));
    const QJsonValue region = o.value(QStringLiteral("region"));
    const QJsonValue row = o.value(QStringLiteral("row"));
    const QJsonValue column = o.value(QStringLiteral("column"));
    const QJsonValue rowSpan = o.value(QStringLiteral("rowSpan"));
    const QJsonValue columnSpan = o.value(QStringLiteral("columnSpan"));
    const QJsonValue sizeMode = o.value(QStringLiteral("sizeMode"));
    const QJsonValue enabled = o.value(QStringLiteral("enabled"));

    if (!widgetId.isString() || !region.isString() || !sizeMode.isString()
        || !row.isDouble() || !column.isDouble() || !rowSpan.isDouble()
        || !columnSpan.isDouble() || !enabled.isBool()) {
        return std::nullopt;
    }

    const std::optional<WidgetId> id = widgetIdFromString(widgetId.toString());
    const std::optional<DeckRegion> reg = regionFromString(region.toString());
    const std::optional<InstrumentSizeMode> size =
        sizeModeFromString(sizeMode.toString());
    if (!id || !reg || !size) {
        return std::nullopt;  // unknown enum token
    }

    DeckWidgetPlacement p;
    p.id = *id;
    p.region = *reg;
    p.row = row.toInt();
    p.column = column.toInt();
    p.rowSpan = rowSpan.toInt();
    p.columnSpan = columnSpan.toInt();
    p.sizeMode = *size;
    p.enabled = enabled.toBool();
    return p;
}

/// Serialise a layout's placements (and pageId) into a JSON object WITHOUT a
/// version field. Used both as the v1 top-level body and as each page entry in
/// a v2 collection.
QJsonObject layoutBodyToJson(const DeckLayout& layout) {
    QJsonObject o;
    o.insert(QStringLiteral("pageId"), layout.pageId);
    QJsonArray placements;
    for (const DeckWidgetPlacement& p : layout.placements) {
        placements.append(placementToJson(p));
    }
    o.insert(QStringLiteral("placements"), placements);
    return o;
}

/// Parse a JSON object's "placements" array (and optional "pageId") into a
/// DeckLayout, no version check. Returns nullopt on a missing/mistyped
/// placements array or any bad placement.
std::optional<DeckLayout> layoutBodyFromJson(const QJsonObject& o) {
    const QJsonValue placements = o.value(QStringLiteral("placements"));
    if (!placements.isArray()) {
        return std::nullopt;
    }
    DeckLayout layout;
    const QJsonValue pageId = o.value(QStringLiteral("pageId"));
    if (pageId.isDouble()) {
        layout.pageId = pageId.toInt();
    }
    const QJsonArray arr = placements.toArray();
    layout.placements.reserve(static_cast<std::size_t>(arr.size()));
    for (const QJsonValue& v : arr) {
        if (!v.isObject()) {
            return std::nullopt;
        }
        const std::optional<DeckWidgetPlacement> p =
            placementFromJson(v.toObject());
        if (!p) {
            return std::nullopt;
        }
        layout.placements.push_back(*p);
    }
    return layout;
}

}  // namespace

QByteArray serializeLayout(const DeckLayout& layout) {
    QJsonObject root = layoutBodyToJson(layout);
    root.insert(QStringLiteral("version"), kLegacyLayoutSchemaVersion);
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

std::optional<DeckLayout> deserializeLayout(const QByteArray& bytes) {
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(bytes, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return std::nullopt;  // malformed JSON
    }
    const QJsonObject root = doc.object();
    const QJsonValue version = root.value(QStringLiteral("version"));
    if (!version.isDouble()
        || version.toInt() != kLegacyLayoutSchemaVersion) {
        return std::nullopt;  // only the legacy v1 shape here
    }
    return layoutBodyFromJson(root);
}

QByteArray serializeCollection(const DeckLayoutCollection& collection) {
    QJsonObject root;
    root.insert(QStringLiteral("version"), kCollectionSchemaVersion);
    root.insert(QStringLiteral("activePageId"), collection.activePageId);
    QJsonArray pages;
    for (const DeckPageDefinition& page : collection.pages) {
        QJsonObject pageObj = layoutBodyToJson(page.layout);
        // pageId/name are the collection's authoritative fields; overwrite the
        // body's pageId with the page definition's id for clarity.
        pageObj.insert(QStringLiteral("pageId"), page.pageId);
        pageObj.insert(QStringLiteral("name"), page.name);
        pages.append(pageObj);
    }
    root.insert(QStringLiteral("pages"), pages);
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

std::optional<DeckLayoutCollection> deserializeCollection(
    const QByteArray& bytes) {
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(bytes, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return std::nullopt;  // malformed JSON
    }
    const QJsonObject root = doc.object();
    const QJsonValue version = root.value(QStringLiteral("version"));
    if (!version.isDouble()) {
        return std::nullopt;  // missing version
    }
    const int v = version.toInt();

    // Legacy v1: a single layout. Migrate it to a full collection.
    if (v == kLegacyLayoutSchemaVersion) {
        const std::optional<DeckLayout> legacy = layoutBodyFromJson(root);
        if (!legacy) {
            return std::nullopt;
        }
        return migrateLegacyLayout(*legacy);
    }

    // Unknown / newer version: fail safely (never interpreted as v2).
    if (v != kCollectionSchemaVersion) {
        return std::nullopt;
    }

    // Version 2: a page collection.
    const QJsonValue activePageId = root.value(QStringLiteral("activePageId"));
    const QJsonValue pages = root.value(QStringLiteral("pages"));
    if (!activePageId.isDouble() || !pages.isArray()) {
        return std::nullopt;
    }
    DeckLayoutCollection collection;
    collection.activePageId = activePageId.toInt();
    const QJsonArray arr = pages.toArray();
    collection.pages.reserve(static_cast<std::size_t>(arr.size()));
    for (const QJsonValue& v2 : arr) {
        if (!v2.isObject()) {
            return std::nullopt;
        }
        const QJsonObject pageObj = v2.toObject();
        const QJsonValue pageId = pageObj.value(QStringLiteral("pageId"));
        const QJsonValue name = pageObj.value(QStringLiteral("name"));
        if (!pageId.isDouble() || !name.isString()) {
            return std::nullopt;
        }
        const std::optional<DeckLayout> layout = layoutBodyFromJson(pageObj);
        if (!layout) {
            return std::nullopt;
        }
        DeckPageDefinition def;
        def.pageId = pageId.toInt();
        def.name = name.toString();
        def.layout = *layout;
        collection.pages.push_back(def);
    }
    return collection;
}

DeckLayoutCollection migrateLegacyLayout(const DeckLayout& legacy) {
    // Preserve the user's v1 layout exactly as "System" (pageId 0), then add the
    // two empty pages, matching the compiled default's shape.
    DeckLayoutCollection c;
    c.activePageId = 0;

    DeckPageDefinition system;
    system.pageId = 0;
    system.name = QStringLiteral("System");
    system.layout = legacy;
    system.layout.pageId = 0;
    c.pages.push_back(system);

    DeckPageDefinition controls;
    controls.pageId = 1;
    controls.name = QStringLiteral("Controls");
    controls.layout = DeckLayout{1, {}};
    c.pages.push_back(controls);

    DeckPageDefinition custom;
    custom.pageId = 2;
    custom.name = QStringLiteral("Custom");
    custom.layout = DeckLayout{2, {}};
    c.pages.push_back(custom);

    return c;
}

}  // namespace darkspark::deck::layout
