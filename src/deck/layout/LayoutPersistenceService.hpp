// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_DECK_LAYOUT_LAYOUTPERSISTENCESERVICE_HPP
#define DARKSPARK_DECK_LAYOUT_LAYOUTPERSISTENCESERVICE_HPP

#include <QString>

#include "deck/layout/DeckLayout.hpp"
#include "deck/layout/DeckLayoutCollection.hpp"

// The file-facing layout persistence service: it owns the on-disk path and all
// file I/O, and hands CommandDeckPage a resolved DeckLayout. The page never sees
// JSON or files. Telemetry is entirely unaware of this. Not a general settings
// framework -- just this one layout file.
namespace darkspark::deck::layout {

class LayoutPersistenceService {
public:
    /// Production constructor: resolves the config file path from
    /// QStandardPaths::AppConfigLocation (org "DarkSpark", app "DarkSpark
    /// Desktop") -> ~/.config/DarkSpark/DarkSpark Desktop/command-deck-layout.json
    LayoutPersistenceService();

    /// Test/override constructor: use an explicit file path so tests never
    /// touch the real config directory.
    explicit LayoutPersistenceService(QString filePath);

    /// The resolved layout file path (for logging/debugging and tests).
    [[nodiscard]] QString filePath() const { return filePath_; }

    /// Resolve the layout for startup, with total fallback and self-recovery:
    ///   * file missing            -> write the compiled default, return it
    ///   * unreadable/malformed    -> log, overwrite with default, return default
    ///   * parsed but invalid      -> log, overwrite with default, return default
    ///   * valid                   -> return it unchanged
    /// Never throws; the Command Deck always gets a usable layout.
    [[nodiscard]] DeckLayout loadOrDefault();

    /// Serialise and write a layout to the file (creating the directory if
    /// needed). Returns false on I/O failure; callers may ignore the result
    /// (a failed save must never prevent launch).
    bool save(const DeckLayout& layout);

    /// Resolve the multi-page COLLECTION for startup, with total fallback,
    /// self-recovery, and one-way v1->v2 migration:
    ///   * file missing            -> write the compiled 3-page default, return it
    ///   * legacy v1 file (valid)  -> migrate to v2 (System keeps the user's
    ///                                layout; add empty Controls/Custom), rewrite
    ///                                as v2, return it
    ///   * unreadable/malformed    -> log, overwrite with 3-page default, return it
    ///   * unknown/newer version   -> treated as invalid: overwrite with default
    ///   * parsed but invalid      -> log, overwrite with default, return default
    ///   * valid v2                -> return it unchanged
    /// Never throws; the Command Deck always gets a usable collection.
    [[nodiscard]] DeckLayoutCollection loadCollectionOrDefault();

    /// Serialise and write a collection as v2. Returns false on I/O failure.
    bool saveCollection(const DeckLayoutCollection& collection);

    /// Persist a new active page id without rewriting semantics beyond it:
    /// re-reads the current collection (or default), updates activePageId, and
    /// writes it back. Intended to be called only on real page switches, not per
    /// frame. Returns false on I/O failure or an invalid target id.
    bool saveActivePage(int activePageId);

private:
    /// Write raw bytes to filePath_, creating the directory if needed. Returns
    /// false on any I/O failure. Shared by save/saveCollection.
    bool writeBytes(const QByteArray& bytes);

    QString filePath_;
};

}  // namespace darkspark::deck::layout

#endif  // DARKSPARK_DECK_LAYOUT_LAYOUTPERSISTENCESERVICE_HPP
