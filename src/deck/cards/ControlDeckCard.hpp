// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_DECK_CARDS_CONTROLDECKCARD_HPP
#define DARKSPARK_DECK_CARDS_CONTROLDECKCARD_HPP

#include "deck/cards/DashboardCard.hpp"
#include "models/ControlAction.hpp"

class QLabel;

namespace darkspark::deck::cards {

/// Large Stream Deck-style launcher grid for the third swipe page.
class ControlDeckCard final : public DashboardCard {
    Q_OBJECT

public:
    explicit ControlDeckCard(QWidget* parent = nullptr);
    void reportResult(bool success, const QString& message);

signals:
    void actionRequested(models::ControlAction action);

private:
    QLabel* mediaDisplay_ = nullptr;
};

}  // namespace darkspark::deck::cards

#endif  // DARKSPARK_DECK_CARDS_CONTROLDECKCARD_HPP
