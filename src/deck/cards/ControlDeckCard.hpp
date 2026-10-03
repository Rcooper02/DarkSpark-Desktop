// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_DECK_CARDS_CONTROLDECKCARD_HPP
#define DARKSPARK_DECK_CARDS_CONTROLDECKCARD_HPP

#include "deck/cards/DashboardCard.hpp"
#include "models/ControlAction.hpp"

class QLabel;
class QWidget;
class QGridLayout;

namespace darkspark::deck::cards {

class GlassActionButton;

/// Large Stream Deck-style launcher grid for the third swipe page.
class ControlDeckCard final : public DashboardCard {
    Q_OBJECT

public:
    explicit ControlDeckCard(QWidget* parent = nullptr);

    void reportResult(
        models::ControlAction action,
        bool success,
        const QString& message);

    void setMediaState(const QString& player,
                       const QString& title,
                       const QString& artist,
                       const QString& album,
                       const QString& artUrl,
                       bool playing);

signals:
    void actionRequested(models::ControlAction action);
    void customActionRequested(const QString& actionType,
                               const QString& target);

private:
    void rebuildDeckGrid();
    void editSlot(int slot);
    void moveSlot(int fromSlot, int toSlot);

    QWidget* keyField_ = nullptr;
    QGridLayout* grid_ = nullptr;
    QLabel* mediaDisplay_ = nullptr;
    GlassActionButton* playPauseButton_ = nullptr;
};

}  // namespace darkspark::deck::cards

#endif
