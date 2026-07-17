// SPDX-License-Identifier: GPL-3.0-or-later
#include "deck/cards/StatusIndicator.hpp"

#include "themes/LegacyTheme.hpp"

#include <QEvent>
#include <QHideEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QShowEvent>
#include <QTimer>

namespace darkspark::deck::cards {

using themes::LegacyTheme;

namespace {
constexpr int kIndicatorSize = 16;    // logical px; scalable via sizeHint
constexpr int kSpinIntervalMs = 60;   // restrained loading cadence
constexpr int kSpinStepDegrees = 30;  // advance per tick
constexpr int kFullCircleDegrees = 360;
// Qt drawArc angles are in sixteenths of a degree.
constexpr int kAnglesPerDegree = 16;
}  // namespace

StatusIndicator::StatusIndicator(QWidget* parent)
    : QWidget(parent), spinTimer_(new QTimer(this)) {
    spinTimer_->setInterval(kSpinIntervalMs);
    connect(spinTimer_, &QTimer::timeout, this, [this]() {
        spinAngle_ = (spinAngle_ + kSpinStepDegrees) % kFullCircleDegrees;
        update();
    });
    setFocusPolicy(Qt::NoFocus);
}

void StatusIndicator::setState(State state) {
    if (state_ == state) {
        return;
    }
    state_ = state;
    updateAnimationState();
    update();
}

StatusIndicator::State StatusIndicator::state() const { return state_; }

QSize StatusIndicator::sizeHint() const { return {kIndicatorSize, kIndicatorSize}; }

QSize StatusIndicator::minimumSizeHint() const { return {kIndicatorSize, kIndicatorSize}; }

QColor StatusIndicator::colorForState() const {
    switch (state_) {
    case State::Normal:
        return LegacyTheme::statusGood();
    case State::Loading:
        return LegacyTheme::accentCyan();
    case State::Empty:
        return LegacyTheme::textSecondary();
    case State::Unavailable:
        return LegacyTheme::textDisabled();
    case State::Warning:
        return LegacyTheme::statusWarning();
    case State::Error:
        return LegacyTheme::statusError();
    case State::Disabled:
        return LegacyTheme::textDisabled();
    }
    return LegacyTheme::textSecondary();
}

void StatusIndicator::updateAnimationState() {
    // Animate only for Loading, and only while visible and enabled.
    const bool shouldAnimate = (state_ == State::Loading) && isVisible() && isEnabled();
    if (shouldAnimate) {
        if (!spinTimer_->isActive()) {
            spinTimer_->start();
        }
    } else {
        if (spinTimer_->isActive()) {
            spinTimer_->stop();
        }
    }
}

void StatusIndicator::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Square drawing area centered in the widget; scales with widget size.
    const int side = qMin(width(), height());
    const qreal inset = side * 0.18;
    const QRectF box(
        (width() - side) / 2.0 + inset, (height() - side) / 2.0 + inset,
        side - 2 * inset, side - 2 * inset);

    const QColor color = colorForState();
    const qreal stroke = qMax(1.5, side * 0.11);

    QPen pen(color);
    pen.setWidthF(stroke);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);

    switch (state_) {
    case State::Normal: {
        // Filled dot.
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        painter.drawEllipse(box);
        break;
    }
    case State::Loading: {
        // Rotating three-quarter arc.
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        const int startAngle = spinAngle_ * kAnglesPerDegree;
        const int spanAngle = 270 * kAnglesPerDegree;
        painter.drawArc(box, startAngle, spanAngle);
        break;
    }
    case State::Empty: {
        // Hollow ring.
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(box);
        break;
    }
    case State::Unavailable: {
        // Horizontal bar (em-dash equivalent), distinct non-round shape.
        painter.setPen(pen);
        const qreal midY = box.center().y();
        painter.drawLine(QPointF(box.left(), midY), QPointF(box.right(), midY));
        break;
    }
    case State::Warning: {
        // Upward triangle.
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        QPolygonF tri;
        tri << QPointF(box.center().x(), box.top())
            << QPointF(box.left(), box.bottom())
            << QPointF(box.right(), box.bottom());
        painter.drawPolygon(tri);
        break;
    }
    case State::Error: {
        // Cross.
        painter.setPen(pen);
        painter.drawLine(box.topLeft(), box.bottomRight());
        painter.drawLine(box.topRight(), box.bottomLeft());
        break;
    }
    case State::Disabled: {
        // Muted hollow ring, thinner.
        QPen thin = pen;
        thin.setWidthF(qMax(1.0, stroke * 0.7));
        painter.setPen(thin);
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(box);
        break;
    }
    }
}

void StatusIndicator::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    updateAnimationState();
}

void StatusIndicator::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    updateAnimationState();
}

void StatusIndicator::changeEvent(QEvent* event) {
    QWidget::changeEvent(event);
    if (event->type() == QEvent::EnabledChange) {
        updateAnimationState();
    }
}

}  // namespace darkspark::deck::cards
