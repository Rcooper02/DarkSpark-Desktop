// SPDX-License-Identifier: GPL-3.0-or-later
#include "deck/navigation/PageIndicator.hpp"

#include "themes/LegacyTheme.hpp"

#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QRadialGradient>
#include <QVariantAnimation>

namespace darkspark::deck::navigation {

using themes::LegacyTheme;

namespace {
constexpr int kDotDiameter = 8;         // quiet inactive marker
constexpr int kActiveDotDiameter = 14;  // active marker: larger, brighter dot
constexpr int kGlowRadius = 9;          // soft illumination around active dot
constexpr int kDotGap = 18;
constexpr int kIndicatorHeight = 28;
// Per-marker slot uses the widest element (active dot + its glow) so hit
// targets are even and comfortably touch-sized.
constexpr int kSlotWidth = kActiveDotDiameter + 2 * kGlowRadius;
}  // namespace

PageIndicator::PageIndicator(QWidget* parent)
    : QWidget(parent), slide_(new QVariantAnimation(this)) {
    setMinimumHeight(kIndicatorHeight);
    slide_->setDuration(LegacyTheme::motionStandard());
    slide_->setEasingCurve(QEasingCurve::InOutCubic);
    connect(slide_, &QVariantAnimation::valueChanged, this,
            [this](const QVariant& value) {
                animatedIndex_ = value.toReal();
                update();
            });
}

void PageIndicator::setState(int pageCount, int activeIndex) {
    pageCount_ = pageCount < 0 ? 0 : pageCount;
    if (pageCount_ == 0) {
        activeIndex_ = 0;
        animatedIndex_ = 0.0;
        update();
        return;
    }
    const int target = qBound(0, activeIndex, pageCount_ - 1);
    activeIndex_ = target;

    // Smoothly animate the active marker from its current animated position to
    // the target. Restarting mid-flight is safe and keeps rapid navigation
    // responsive (no queue of animations builds up).
    slide_->stop();
    slide_->setStartValue(animatedIndex_);
    slide_->setEndValue(static_cast<qreal>(target));
    slide_->start();
}

int PageIndicator::pageCount() const { return pageCount_; }
int PageIndicator::activeIndex() const { return activeIndex_; }

int PageIndicator::slotStride() const { return kSlotWidth + kDotGap; }

int PageIndicator::rowSpan() const {
    if (pageCount_ <= 0) {
        return 0;
    }
    return pageCount_ * kSlotWidth + (pageCount_ - 1) * kDotGap;
}

int PageIndicator::rowStartX() const { return (width() - rowSpan()) / 2; }

QSize PageIndicator::sizeHint() const { return {rowSpan(), kIndicatorHeight}; }

void PageIndicator::paintEvent(QPaintEvent* /*event*/) {
    if (pageCount_ <= 0) {
        return;
    }
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const int startX = rowStartX();
    const int centerY = height() / 2;
    const int stride = slotStride();

    // Quiet inactive dots.
    painter.setPen(Qt::NoPen);
    painter.setBrush(LegacyTheme::borderStrong());
    for (int i = 0; i < pageCount_; ++i) {
        const int slotCenterX = startX + i * stride + kSlotWidth / 2;
        painter.drawEllipse(QPoint(slotCenterX, centerY), kDotDiameter / 2,
                            kDotDiameter / 2);
    }

    // Active illuminated dot at the animated position (slides between slots).
    const qreal activeCenterX =
        startX + animatedIndex_ * stride + kSlotWidth / 2.0;
    const QPointF activeCenter(activeCenterX, centerY);

    // Soft illumination: a low-opacity cyan halo behind the dot (controlled
    // glow, docs/VISUAL_LANGUAGE.md — an accent, not a full-radius blur).
    QColor halo = LegacyTheme::accentCyan();
    QRadialGradient gradient(activeCenter, kActiveDotDiameter / 2.0 + kGlowRadius);
    QColor haloInner = halo;
    haloInner.setAlpha(110);
    QColor haloOuter = halo;
    haloOuter.setAlpha(0);
    gradient.setColorAt(0.0, haloInner);
    gradient.setColorAt(1.0, haloOuter);
    painter.setPen(Qt::NoPen);
    painter.setBrush(gradient);
    painter.drawEllipse(activeCenter, kActiveDotDiameter / 2.0 + kGlowRadius,
                        kActiveDotDiameter / 2.0 + kGlowRadius);

    // The bright dot itself.
    painter.setBrush(LegacyTheme::accentCyan());
    painter.drawEllipse(activeCenter, kActiveDotDiameter / 2.0,
                        kActiveDotDiameter / 2.0);
}

void PageIndicator::mouseReleaseEvent(QMouseEvent* event) {
    if (pageCount_ <= 0) {
        QWidget::mouseReleaseEvent(event);
        return;
    }
    const int startX = rowStartX();
    const int stride = slotStride();
    const int relative = event->pos().x() - startX;
    if (relative >= 0 && stride > 0) {
        const int index = relative / stride;
        if (index >= 0 && index < pageCount_) {
            emit pageRequested(index);
            return;
        }
    }
    QWidget::mouseReleaseEvent(event);
}

}  // namespace darkspark::deck::navigation
