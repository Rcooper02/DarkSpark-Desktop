// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_DECK_CARDS_FEISHINCARD_HPP
#define DARKSPARK_DECK_CARDS_FEISHINCARD_HPP

#include "deck/cards/DashboardCard.hpp"

class QWebEngineProfile;
class QWebEngineView;

namespace darkspark::deck::cards {

class FeishinCard final : public DashboardCard {
    Q_OBJECT

public:
    explicit FeishinCard(QWidget* parent = nullptr);

private:
    QWebEngineProfile* profile_ = nullptr;
    QWebEngineView* view_ = nullptr;
};

}  // namespace darkspark::deck::cards

#endif
