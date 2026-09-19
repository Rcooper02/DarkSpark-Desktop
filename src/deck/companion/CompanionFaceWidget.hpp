// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_DECK_COMPANION_COMPANIONFACEWIDGET_HPP
#define DARKSPARK_DECK_COMPANION_COMPANIONFACEWIDGET_HPP

#include "models/CompanionState.hpp"
#include "models/GazeTarget.hpp"

#include <QElapsedTimer>
#include <QWidget>

class QPaintEvent;
class QTimer;

namespace darkspark::deck::companion {

/// Animated, presentation-only optical lens for the DarkSpark Companion.
///
/// It renders a supplied CompanionState and owns only animation timing. It
/// does not open cameras or microphones and contains no recognition or AI.
class CompanionFaceWidget final : public QWidget {
    Q_OBJECT

public:
    explicit CompanionFaceWidget(QWidget* parent = nullptr);

    void setCompanionState(models::CompanionState state);
    [[nodiscard]] models::CompanionState companionState() const;
    void setGazeTarget(models::GazeTarget target);
    void clearGazeTarget();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    [[nodiscard]] double animationSeconds() const;

    models::CompanionState state_ = models::CompanionState::Idle;

    // Camera-requested target and the deliberately slower rendered gaze.
    models::GazeTarget gazeTarget_;
    models::GazeTarget renderedGaze_;
    bool hasGazeTarget_ = false;
    QTimer* animationTimer_;
    QElapsedTimer elapsed_;
};

}  // namespace darkspark::deck::companion

#endif  // DARKSPARK_DECK_COMPANION_COMPANIONFACEWIDGET_HPP
