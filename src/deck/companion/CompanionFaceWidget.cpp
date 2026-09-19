// SPDX-License-Identifier: GPL-3.0-or-later
#include "deck/companion/CompanionFaceWidget.hpp"

#include "themes/LegacyTheme.hpp"

#include <QPainter>
#include <QTimer>

#include <algorithm>
#include <cmath>

namespace darkspark::deck::companion {

using models::CompanionState;
using themes::LegacyTheme;

CompanionFaceWidget::CompanionFaceWidget(QWidget* parent)
    : QWidget(parent), animationTimer_(new QTimer(this)) {
    setMinimumSize(340, 280);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    elapsed_.start();
    animationTimer_->setInterval(16);
    connect(animationTimer_, &QTimer::timeout, this,
            qOverload<>(&CompanionFaceWidget::update));
    animationTimer_->start();
}

void CompanionFaceWidget::setCompanionState(CompanionState state) {
    if (state_ == state) {
        return;
    }
    state_ = state;
    elapsed_.restart();
    update();
}

CompanionState CompanionFaceWidget::companionState() const { return state_; }

void CompanionFaceWidget::setGazeTarget(models::GazeTarget target) {
    target = target.clamped();

    // Ignore tiny detector fluctuations. The camera can move a few pixels
    // even when a person is standing still; HAL should not twitch with it.
    constexpr double kDeadZone = 0.035;

    if (hasGazeTarget_ &&
        std::abs(target.horizontal - gazeTarget_.horizontal) < kDeadZone &&
        std::abs(target.vertical - gazeTarget_.vertical) < kDeadZone) {
        return;
    }

    gazeTarget_ = target;
    hasGazeTarget_ = true;
}

void CompanionFaceWidget::clearGazeTarget() {
    hasGazeTarget_ = false;
    update();
}

double CompanionFaceWidget::animationSeconds() const {
    return static_cast<double>(elapsed_.elapsed()) / 1000.0;
}

void CompanionFaceWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const double seconds = animationSeconds();
    const QRectF bounds = rect().adjusted(20, 14, -20, -14);
    const qreal diameter = std::min(bounds.width(), bounds.height()) * 0.88;
    const QPointF housingCenter = bounds.center();
    const QRectF housing(housingCenter.x() - diameter / 2.0,
                         housingCenter.y() - diameter / 2.0, diameter, diameter);

    QRadialGradient bezel(housingCenter, diameter / 2.0);
    bezel.setColorAt(0.0, QColor(36, 39, 43));
    bezel.setColorAt(0.68, QColor(14, 17, 20));
    bezel.setColorAt(0.86, QColor(4, 5, 7));
    bezel.setColorAt(1.0, LegacyTheme::borderStrong());
    painter.setBrush(bezel);
    painter.setPen(QPen(LegacyTheme::borderStrong(), 2.0));
    painter.drawEllipse(housing);

    const qreal lensDiameter = diameter * 0.69;
    const QRectF lens(housingCenter.x() - lensDiameter / 2.0,
                      housingCenter.y() - lensDiameter / 2.0,
                      lensDiameter, lensDiameter);

    const double pulseSpeed = state_ == CompanionState::Listening ? 4.5 : 1.6;
    const double pulse = 0.86 + std::sin(seconds * pulseSpeed) * 0.10;
    double intensity = state_ == CompanionState::Dormant ? 0.16 : pulse;
    if (state_ == CompanionState::Alert) {
        intensity = 0.88 + std::abs(std::sin(seconds * 7.0)) * 0.12;
    }

    // HAL-style gaze behaviour:
    // camera data selects a destination, but the rendered eye deliberately
    // glides toward it rather than mirroring every detector update.
    const models::GazeTarget desiredGaze =
        hasGazeTarget_
            ? gazeTarget_
            : models::GazeTarget{
                  std::sin(seconds * 0.48) * 0.34,
                  std::sin(seconds * 0.31) * 0.16};

    // Exponential easing. At ~60 Hz this produces a noticeable, deliberate
    // movement rather than webcam-like snapping.
    constexpr double kGazeEase = 0.045;

    renderedGaze_.horizontal +=
        (desiredGaze.horizontal - renderedGaze_.horizontal) * kGazeEase;
    renderedGaze_.vertical +=
        (desiredGaze.vertical - renderedGaze_.vertical) * kGazeEase;

    const QPointF opticalCenter(
        housingCenter.x() + renderedGaze_.horizontal * lensDiameter * 0.14,
        housingCenter.y() + renderedGaze_.vertical * lensDiameter * 0.11);

    QRadialGradient lensGlow(opticalCenter, lensDiameter * 0.58);
    lensGlow.setColorAt(0.0, QColor(255, 246, 220,
                                   static_cast<int>(255.0 * intensity)));
    lensGlow.setColorAt(0.10, QColor(255, 88, 58,
                                    static_cast<int>(250.0 * intensity)));
    lensGlow.setColorAt(0.35, QColor(220, 0, 12,
                                    static_cast<int>(235.0 * intensity)));
    lensGlow.setColorAt(0.72, QColor(82, 0, 7,
                                    static_cast<int>(220.0 * intensity)));
    lensGlow.setColorAt(1.0, QColor(15, 0, 2, 245));
    painter.setBrush(lensGlow);
    painter.setPen(QPen(QColor(122, 14, 18), 2.0));
    painter.drawEllipse(lens);

    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(255, 45, 50,
                              static_cast<int>(150.0 * intensity)), 2.0));
    painter.drawEllipse(lens.adjusted(lensDiameter * 0.13, lensDiameter * 0.13,
                                      -lensDiameter * 0.13, -lensDiameter * 0.13));
    painter.drawEllipse(lens.adjusted(lensDiameter * 0.28, lensDiameter * 0.28,
                                      -lensDiameter * 0.28, -lensDiameter * 0.28));

    const qreal coreRadius = lensDiameter * 0.095;
    QRadialGradient core(opticalCenter, coreRadius);
    core.setColorAt(0.0, QColor(255, 255, 238));
    core.setColorAt(0.32, QColor(255, 222, 168));
    core.setColorAt(1.0, QColor(255, 32, 28, 20));
    painter.setPen(Qt::NoPen);
    painter.setBrush(core);
    painter.drawEllipse(opticalCenter, coreRadius, coreRadius);

    painter.setBrush(QColor(255, 255, 255,
                            state_ == CompanionState::Dormant ? 18 : 105));
    painter.drawEllipse(opticalCenter + QPointF(-coreRadius * 0.35,
                                                -coreRadius * 0.38),
                        coreRadius * 0.20, coreRadius * 0.20);
}

}  // namespace darkspark::deck::companion
