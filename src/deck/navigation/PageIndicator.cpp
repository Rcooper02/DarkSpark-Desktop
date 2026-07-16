// SPDX-License-Identifier: GPL-3.0-or-later
#include "deck/navigation/PageIndicator.hpp"

#include "themes/LegacyTheme.hpp"

#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>

namespace darkspark::deck::navigation {

using themes::LegacyTheme;

namespace {
constexpr int kDotDiameter = 10;
constexpr int kDotGap = 14;
constexpr int kActiveDotDiameter = 14;
constexpr int kIndicatorHeight = 28;
}  // namespace

PageIndicator::PageIndicator(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(kIndicatorHeight);
}

void PageIndicator::setState(int pageCount, int activeIndex) {
    pageCount_ = pageCount < 0 ? 0 : pageCount;
    if (pageCount_ == 0) {
        activeIndex_ = 0;
    } else {
        activeIndex_ = qBound(0, activeIndex, pageCount_ - 1);
    }
    update();
}

int PageIndicator::pageCount() const { return pageCount_; }

int PageIndicator::activeIndex() const { return activeIndex_; }

int PageIndicator::dotSpanForHitTest() const {
    // Total horizontal span occupied by the dot row, using the largest dot so
    // hit regions comfortably cover touch input.
    if (pageCount_ <= 0) {
        return 0;
    }
    return pageCount_ * kActiveDotDiameter + (pageCount_ - 1) * kDotGap;
}

QSize PageIndicator::sizeHint() const {
    return {dotSpanForHitTest(), kIndicatorHeight};
}

void PageIndicator::paintEvent(QPaintEvent* /*event*/) {
    if (pageCount_ <= 0) {
        return;
    }
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const int span = dotSpanForHitTest();
    int x = (width() - span) / 2;
    const int centerY = height() / 2;

    for (int i = 0; i < pageCount_; ++i) {
        const bool active = (i == activeIndex_);
        const int diameter = active ? kActiveDotDiameter : kDotDiameter;
        const QColor color = active ? LegacyTheme::accentCyan() : LegacyTheme::borderSubtle();

        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        // Center each dot within an active-diameter slot for even spacing.
        const int slotCenterX = x + kActiveDotDiameter / 2;
        painter.drawEllipse(QPoint(slotCenterX, centerY), diameter / 2, diameter / 2);
        x += kActiveDotDiameter + kDotGap;
    }
}

void PageIndicator::mouseReleaseEvent(QMouseEvent* event) {
    if (pageCount_ <= 0) {
        QWidget::mouseReleaseEvent(event);
        return;
    }
    const int span = dotSpanForHitTest();
    const int startX = (width() - span) / 2;
    const int slot = kActiveDotDiameter + kDotGap;
    const int relative = event->pos().x() - startX;
    if (relative >= 0 && slot > 0) {
        const int index = relative / slot;
        if (index >= 0 && index < pageCount_) {
            emit pageRequested(index);
            return;
        }
    }
    QWidget::mouseReleaseEvent(event);
}

}  // namespace darkspark::deck::navigation
