// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_MODELS_DECKBUTTONCONFIG_HPP
#define DARKSPARK_MODELS_DECKBUTTONCONFIG_HPP

#include <QString>

namespace darkspark::models {

/// Persistent configuration for one Control Deck slot.
struct DeckButtonConfig {
    int slot = -1;
    QString title;
    QString iconPath;
    QString actionType;
    QString target;

    [[nodiscard]] bool isConfigured() const {
        return slot >= 0 && !actionType.trimmed().isEmpty();
    }
};

}  // namespace darkspark::models

#endif  // DARKSPARK_MODELS_DECKBUTTONCONFIG_HPP
