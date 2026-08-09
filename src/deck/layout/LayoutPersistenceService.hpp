// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_DECK_LAYOUT_LAYOUTPERSISTENCESERVICE_HPP
#define DARKSPARK_DECK_LAYOUT_LAYOUTPERSISTENCESERVICE_HPP

#include <QString>

#include "deck/layout/DeckLayout.hpp"

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

private:
    QString filePath_;
};

}  // namespace darkspark::deck::layout

#endif  // DARKSPARK_DECK_LAYOUT_LAYOUTPERSISTENCESERVICE_HPP
