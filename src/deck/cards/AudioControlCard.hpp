// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_DECK_CARDS_AUDIOCONTROLCARD_HPP
#define DARKSPARK_DECK_CARDS_AUDIOCONTROLCARD_HPP

#include "deck/cards/DashboardCard.hpp"
#include "models/ControlAction.hpp"

namespace darkspark::deck::cards {

/// Reusable playback and PipeWire volume control surface.
class AudioControlCard final : public DashboardCard {
    Q_OBJECT

public:
    explicit AudioControlCard(QWidget* parent = nullptr);
    void reportResult(bool success, const QString& message);

signals:
    void actionRequested(models::ControlAction action);
};

}  // namespace darkspark::deck::cards

#endif  // DARKSPARK_DECK_CARDS_AUDIOCONTROLCARD_HPP
