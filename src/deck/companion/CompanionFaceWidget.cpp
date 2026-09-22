// SPDX-License-Identifier: GPL-3.0-or-later
#include "deck/companion/CompanionFaceWidget.hpp"

#include "themes/LegacyTheme.hpp"

#include <QConicalGradient>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
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

CompanionState CompanionFaceWidget::companionState() const {
    return state_;
}

void CompanionFaceWidget::setGazeTarget(models::GazeTarget target) {
    target = target.clamped();

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

    const QRectF bounds = rect().adjusted(14, 10, -14, -10);

    const qreal diameter =
        std::min(bounds.width(), bounds.height()) * 0.96;

    const QPointF center = bounds.center();

    const QRectF outer(
        center.x() - diameter / 2.0,
        center.y() - diameter / 2.0,
        diameter,
        diameter);

    // -----------------------------------------------------------------
    // OUTER MACHINED METAL HOUSING
    // -----------------------------------------------------------------

    // Radial depth establishes the rolled outer rim.
    QRadialGradient outerMetal(center, diameter * 0.52);
    outerMetal.setColorAt(0.00, QColor(28, 30, 33));
    outerMetal.setColorAt(0.61, QColor(18, 20, 23));
    outerMetal.setColorAt(0.70, QColor(68, 72, 76));
    outerMetal.setColorAt(0.76, QColor(226, 230, 232));
    outerMetal.setColorAt(0.81, QColor(92, 97, 101));
    outerMetal.setColorAt(0.89, QColor(30, 33, 36));
    outerMetal.setColorAt(0.96, QColor(174, 179, 182));
    outerMetal.setColorAt(1.00, QColor(20, 22, 24));

    painter.setPen(QPen(QColor(5, 7, 9), 3.0));
    painter.setBrush(outerMetal);
    painter.drawEllipse(outer);

    // Directional bands make the housing read as turned, reflective metal
    // instead of flat grey concentric circles.
    QConicalGradient brushedMetal(center, -32.0);
    brushedMetal.setColorAt(0.00, QColor(250, 252, 253, 190));
    brushedMetal.setColorAt(0.08, QColor(76, 81, 85, 150));
    brushedMetal.setColorAt(0.18, QColor(214, 218, 220, 175));
    brushedMetal.setColorAt(0.31, QColor(52, 56, 60, 155));
    brushedMetal.setColorAt(0.45, QColor(238, 241, 242, 180));
    brushedMetal.setColorAt(0.59, QColor(67, 72, 76, 160));
    brushedMetal.setColorAt(0.73, QColor(196, 201, 204, 175));
    brushedMetal.setColorAt(0.87, QColor(43, 47, 51, 165));
    brushedMetal.setColorAt(1.00, QColor(250, 252, 253, 190));
    painter.setPen(Qt::NoPen);
    painter.setBrush(brushedMetal);
    painter.drawEllipse(outer.adjusted(diameter * 0.018, diameter * 0.018,
                                       -diameter * 0.018, -diameter * 0.018));

    // A diagonal specular pass gives the metal a clear light source.
    QLinearGradient metalSheen(outer.topLeft(), outer.bottomRight());
    metalSheen.setColorAt(0.00, QColor(255, 255, 255, 150));
    metalSheen.setColorAt(0.34, QColor(255, 255, 255, 22));
    metalSheen.setColorAt(0.66, QColor(0, 0, 0, 105));
    metalSheen.setColorAt(1.00, QColor(226, 232, 235, 75));
    painter.setBrush(metalSheen);
    painter.drawEllipse(outer.adjusted(diameter * 0.035, diameter * 0.035,
                                       -diameter * 0.035, -diameter * 0.035));

    // One restrained bevel defines a single machined silver bezel.
    painter.setBrush(Qt::NoBrush);
    const QRectF bezelEdge =
        outer.adjusted(diameter * 0.075, diameter * 0.075,
                       -diameter * 0.075, -diameter * 0.075);
    painter.setPen(QPen(QColor(242, 246, 248, 210), 2.5));
    painter.drawArc(bezelEdge, 28 * 16, 180 * 16);
    painter.setPen(QPen(QColor(24, 27, 30, 235), 3.0));
    painter.drawArc(bezelEdge, 208 * 16, 180 * 16);

    // -----------------------------------------------------------------
    // BLACK RECESSED OPTICAL CAVITY
    // -----------------------------------------------------------------

    const qreal cavityDiameter = diameter * 0.69;

    const QRectF cavity(
        center.x() - cavityDiameter / 2.0,
        center.y() - cavityDiameter / 2.0,
        cavityDiameter,
        cavityDiameter);

    QRadialGradient cavityGradient(center, cavityDiameter * 0.50);

    cavityGradient.setColorAt(0.0, QColor(16, 4, 5));
    cavityGradient.setColorAt(0.65, QColor(7, 4, 5));
    cavityGradient.setColorAt(0.86, QColor(3, 4, 5));
    cavityGradient.setColorAt(1.0, QColor(0, 0, 0));

    painter.setPen(QPen(QColor(0, 0, 0), 5.0));
    painter.setBrush(cavityGradient);
    painter.drawEllipse(cavity);

    // -----------------------------------------------------------------
    // GAZE + HAL MOTION
    // -----------------------------------------------------------------

    const models::GazeTarget desiredGaze =
        hasGazeTarget_
            ? gazeTarget_
            : models::GazeTarget{
                  std::sin(seconds * 0.48) * 0.34,
                  std::sin(seconds * 0.31) * 0.16};

    constexpr double kGazeEase = 0.045;

    renderedGaze_.horizontal +=
        (desiredGaze.horizontal - renderedGaze_.horizontal) * kGazeEase;

    renderedGaze_.vertical +=
        (desiredGaze.vertical - renderedGaze_.vertical) * kGazeEase;

    // -----------------------------------------------------------------
    // INNER LENS
    // -----------------------------------------------------------------

    const qreal lensDiameter = cavityDiameter * 0.84;

    const QPointF opticalCenter(
        center.x() +
            renderedGaze_.horizontal * lensDiameter * 0.12,
        center.y() +
            renderedGaze_.vertical * lensDiameter * 0.09);

    const QRectF lens(
        center.x() - lensDiameter / 2.0,
        center.y() - lensDiameter / 2.0,
        lensDiameter,
        lensDiameter);

    const double pulseSpeed =
        state_ == CompanionState::Listening ? 4.5 : 1.6;

    const double pulse =
        0.88 + std::sin(seconds * pulseSpeed) * 0.075;

    double intensity =
        state_ == CompanionState::Dormant ? 0.18 : pulse;

    if (state_ == CompanionState::Alert) {
        intensity =
            0.90 + std::abs(std::sin(seconds * 7.0)) * 0.10;
    }

    QRadialGradient lensBody(opticalCenter, lensDiameter * 0.55);

    lensBody.setColorAt(
        0.00,
        QColor(
            255,
            244,
            210,
            static_cast<int>(245.0 * intensity)));

    lensBody.setColorAt(
        0.06,
        QColor(
            255,
            96,
            48,
            static_cast<int>(245.0 * intensity)));

    lensBody.setColorAt(
        0.18,
        QColor(
            230,
            8,
            16,
            static_cast<int>(245.0 * intensity)));

    lensBody.setColorAt(
        0.40,
        QColor(
            145,
            0,
            7,
            static_cast<int>(240.0 * intensity)));

    lensBody.setColorAt(
        0.68,
        QColor(
            57,
            0,
            4,
            static_cast<int>(245.0 * intensity)));

    lensBody.setColorAt(
        0.90,
        QColor(15, 0, 2, 250));

    lensBody.setColorAt(
        1.00,
        QColor(3, 0, 1, 255));

    painter.setPen(QPen(QColor(130, 10, 12), 2.5));
    painter.setBrush(lensBody);
    painter.drawEllipse(lens);

    // -----------------------------------------------------------------
    // SHARP OPTICAL RINGS
    // -----------------------------------------------------------------

    painter.setBrush(Qt::NoBrush);

    const qreal ringWidth = std::max<qreal>(1.0, lensDiameter * 0.004);

    // Two quiet optical traces add depth without competing with the hot core.
    painter.setPen(
        QPen(QColor(255, 54, 52, static_cast<int>(72.0 * intensity)),
             ringWidth));
    painter.drawEllipse(
        lens.adjusted(lensDiameter * 0.19, lensDiameter * 0.19,
                      -lensDiameter * 0.19, -lensDiameter * 0.19));

    painter.setPen(
        QPen(QColor(255, 90, 68, static_cast<int>(48.0 * intensity)),
             ringWidth));
    painter.drawEllipse(
        lens.adjusted(lensDiameter * 0.34, lensDiameter * 0.34,
                      -lensDiameter * 0.34, -lensDiameter * 0.34));

    // -----------------------------------------------------------------
    // HOT CENTRAL APERTURE
    // -----------------------------------------------------------------

    const qreal coreRadius = lensDiameter * 0.075;

    QRadialGradient core(opticalCenter, coreRadius);

    core.setColorAt(0.0, QColor(255, 255, 244));
    core.setColorAt(0.18, QColor(255, 246, 198));
    core.setColorAt(0.48, QColor(255, 160, 70));
    core.setColorAt(0.75, QColor(255, 44, 22));
    core.setColorAt(1.0, QColor(150, 0, 5, 10));

    painter.setPen(Qt::NoPen);
    painter.setBrush(core);

    painter.drawEllipse(
        opticalCenter,
        coreRadius,
        coreRadius);

    // Tiny sharp white center.
    painter.setBrush(
        QColor(
            255,
            255,
            248,
            state_ == CompanionState::Dormant ? 30 : 245));

    painter.drawEllipse(
        opticalCenter,
        coreRadius * 0.20,
        coreRadius * 0.20);

    // -----------------------------------------------------------------
    // CLEAN OPTICAL LENS
    // No glass overlay: keep HAL's optics crisp and unobstructed.
    // -----------------------------------------------------------------

    painter.setBrush(Qt::NoBrush);

    painter.setPen(
        QPen(
            QColor(150, 18, 20, 115),
            1.25));

    painter.drawEllipse(lens);

}

}  // namespace darkspark::deck::companion
