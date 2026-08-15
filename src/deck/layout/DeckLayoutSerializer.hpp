// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_DECK_LAYOUT_DECKLAYOUTSERIALIZER_HPP
#define DARKSPARK_DECK_LAYOUT_DECKLAYOUTSERIALIZER_HPP

#include <optional>

#include <QByteArray>
#include <QString>

#include "deck/layout/DeckLayout.hpp"
#include "deck/layout/DeckLayoutCollection.hpp"

// Serialization for the pure-data layout types: DeckLayout / DeckLayoutCollection
// <-> JSON, plus the enum<->string mappings the persistence batch defers here
// (they stay out of the pure model). This component does NO file I/O -- it only
// converts data to/from bytes, so it is fully testable without touching disk.
// The persistence service owns files and paths.
namespace darkspark::deck::layout {

/// Current on-disk schema version: 2 (a page collection). Version 1 was a single
/// DeckLayout; it is still readable for one-way migration to v2. An unknown or
/// newer version fails to parse (nullopt) so the caller falls back to the
/// compiled default -- unknown future versions are never interpreted as v2.
inline constexpr int kCollectionSchemaVersion = 2;
inline constexpr int kLegacyLayoutSchemaVersion = 1;

// --- enum <-> string mappings (the deferred Batch-1 mappings) ----------------
// Each toString is total; each fromString returns nullopt for an unrecognised
// token so an unknown value triggers graceful fallback rather than a crash.
// WidgetId::Unknown has no string form: it is the failure sentinel, never
// serialised, and an unknown token maps to nullopt (not to Unknown).

[[nodiscard]] QString widgetIdToString(WidgetId id);
[[nodiscard]] std::optional<WidgetId> widgetIdFromString(const QString& s);

[[nodiscard]] QString regionToString(DeckRegion region);
[[nodiscard]] std::optional<DeckRegion> regionFromString(const QString& s);

[[nodiscard]] QString sizeModeToString(InstrumentSizeMode mode);
[[nodiscard]] std::optional<InstrumentSizeMode> sizeModeFromString(
    const QString& s);

// --- DeckLayout <-> JSON bytes (single page; also used inside collections) ---

/// Serialise a single layout to pretty-printed JSON bytes. Total.
[[nodiscard]] QByteArray serializeLayout(const DeckLayout& layout);

/// Parse JSON bytes into a DeckLayout (legacy v1 top-level, or a nested page
/// object without a version field -- see deserializeCollection). Returns nullopt
/// on malformed JSON, unknown version, missing/mistyped fields, or unknown enum
/// strings. Does NOT run isValidLayout.
[[nodiscard]] std::optional<DeckLayout> deserializeLayout(
    const QByteArray& bytes);

// --- DeckLayoutCollection <-> JSON bytes (schema version 2) ------------------

/// Serialise a collection to pretty-printed v2 JSON: { version:2, activePageId,
/// pages:[{pageId,name,placements:[...]}, ...] }. Total.
[[nodiscard]] QByteArray serializeCollection(
    const DeckLayoutCollection& collection);

/// Parse JSON bytes into a collection, honouring the schema version:
///   * version 2 -> parsed as a collection,
///   * version 1 -> migrated to a collection (legacy single layout becomes the
///                  first page; caller supplies extra default pages via
///                  migrateLegacyLayout, not here),
///   * anything else / malformed -> nullopt.
/// Returns nullopt for unknown enums, missing/mistyped fields, or unknown
/// version. Does NOT run isValidCollection -- the caller validates.
[[nodiscard]] std::optional<DeckLayoutCollection> deserializeCollection(
    const QByteArray& bytes);

/// Migrate a legacy v1 single layout into a full v2 collection: the v1 layout
/// becomes "System" (pageId 0), and empty "Controls" (1) and "Custom" (2) pages
/// are appended, with activePageId 0. This preserves a valid user's v1 layout
/// exactly rather than discarding it.
[[nodiscard]] DeckLayoutCollection migrateLegacyLayout(
    const DeckLayout& legacy);

}  // namespace darkspark::deck::layout

#endif  // DARKSPARK_DECK_LAYOUT_DECKLAYOUTSERIALIZER_HPP
