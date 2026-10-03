// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_DECK_CONFIG_DECKLAYOUTSTORE_HPP
#define DARKSPARK_DECK_CONFIG_DECKLAYOUTSTORE_HPP

#include "models/DeckButtonConfig.hpp"

#include <QVector>

namespace darkspark::deck::config {

class DeckLayoutStore final {
public:
    static constexpr int SlotCount = 30;

    [[nodiscard]] static QVector<models::DeckButtonConfig> load();
    static bool save(const QVector<models::DeckButtonConfig>& buttons);

    [[nodiscard]] static QString layoutPath();

private:
    [[nodiscard]] static QVector<models::DeckButtonConfig> defaults();
};

}  // namespace darkspark::deck::config

#endif  // DARKSPARK_DECK_CONFIG_DECKLAYOUTSTORE_HPP
