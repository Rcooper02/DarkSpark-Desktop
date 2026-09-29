// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_DECK_COMPANION_COMPANIONCARD_HPP
#define DARKSPARK_DECK_COMPANION_COMPANIONCARD_HPP

#include "deck/cards/DashboardCard.hpp"
#include "models/CompanionState.hpp"
#include "models/GazeTarget.hpp"

class QLabel;

namespace darkspark::deck::companion {

class CompanionFaceWidget;

/// Touch-friendly Companion presentation and development controls.
///
/// The state buttons are an intentional hardware-independent test harness for
/// Companion-0. They emit user intent; camera/audio/AI services are not owned
/// or contacted by this card.
class CompanionCard final : public cards::DashboardCard {
    Q_OBJECT

public:
    explicit CompanionCard(QWidget* parent = nullptr);

    void setCompanionState(models::CompanionState state);
    [[nodiscard]] models::CompanionState companionState() const;
    void setGazeTarget(models::GazeTarget target);
    void clearGazeTarget();

signals:
    void stateRequested(models::CompanionState state);

private:
    CompanionFaceWidget* face_;
    QLabel* stateLabel_;
};

}  // namespace darkspark::deck::companion

#endif  // DARKSPARK_DECK_COMPANION_COMPANIONCARD_HPP
