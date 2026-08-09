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

}  // namespace

QByteArray serializeLayout(const DeckLayout& layout) {
    QJsonObject root;
    root.insert(QStringLiteral("version"), kLayoutSchemaVersion);
    root.insert(QStringLiteral("pageId"), layout.pageId);
    QJsonArray placements;
    for (const DeckWidgetPlacement& p : layout.placements) {
        placements.append(placementToJson(p));
    }
    root.insert(QStringLiteral("placements"), placements);
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
    if (!version.isDouble() || version.toInt() != kLayoutSchemaVersion) {
        return std::nullopt;  // missing/unknown/newer version -> fallback
    }

    const QJsonValue placements = root.value(QStringLiteral("placements"));
    if (!placements.isArray()) {
        return std::nullopt;
    }

    DeckLayout layout;
    // pageId is optional and defaults to 0; accept an integer if present.
    const QJsonValue pageId = root.value(QStringLiteral("pageId"));
    if (pageId.isDouble()) {
        layout.pageId = pageId.toInt();
    }

    const QJsonArray arr = placements.toArray();
    layout.placements.reserve(static_cast<std::size_t>(arr.size()));
    for (const QJsonValue& v : arr) {
        if (!v.isObject()) {
            return std::nullopt;
        }
        const std::optional<DeckWidgetPlacement> p = placementFromJson(v.toObject());
        if (!p) {
            return std::nullopt;  // any bad placement invalidates the whole file
        }
        layout.placements.push_back(*p);
    }
    return layout;
}

}  // namespace darkspark::deck::layout
