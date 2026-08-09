// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_DECK_LAYOUT_DECKLAYOUTSERIALIZER_HPP
#define DARKSPARK_DECK_LAYOUT_DECKLAYOUTSERIALIZER_HPP

#include <optional>

#include <QByteArray>
#include <QString>

#include "deck/layout/DeckLayout.hpp"

// Serialization for the pure-data DeckLayout: DeckLayout <-> JSON, plus the
// enum<->string mappings the persistence batch defers here (they stay out of the
// pure model so DeckLayout keeps no string/JSON knowledge). This component does
// NO file I/O -- it only converts data to/from bytes, so it is fully testable
// without touching disk. The persistence service owns files and paths.
namespace darkspark::deck::layout {

/// The on-disk schema version. Bumped only when the JSON shape changes. Batch 2
/// writes and accepts version 1; an unknown/newer version fails to parse
/// (nullopt) so the caller falls back to the compiled default. Real migration
/// logic is deferred.
inline constexpr int kLayoutSchemaVersion = 1;

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

// --- DeckLayout <-> JSON bytes ----------------------------------------------

/// Serialise a layout to pretty-printed JSON bytes (human-readable, with the
/// "version" field). Total: any valid DeckLayout serialises.
[[nodiscard]] QByteArray serializeLayout(const DeckLayout& layout);

/// Parse JSON bytes into a DeckLayout. Returns nullopt when the bytes are not
/// valid JSON, the version is unknown, a required field is missing or the wrong
/// type, or any enum string is unrecognised. Does NOT run isValidLayout -- the
/// caller decides whether to additionally validate geometry/overlap. Keeping
/// parse and validate separate lets tests distinguish "unparseable" from
/// "parsed but geometrically invalid".
[[nodiscard]] std::optional<DeckLayout> deserializeLayout(
    const QByteArray& bytes);

}  // namespace darkspark::deck::layout

#endif  // DARKSPARK_DECK_LAYOUT_DECKLAYOUTSERIALIZER_HPP
