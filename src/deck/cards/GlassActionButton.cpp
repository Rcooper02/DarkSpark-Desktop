// SPDX-License-Identifier: GPL-3.0-or-later
#include "deck/cards/GlassActionButton.hpp"

#include "themes/LegacyTheme.hpp"

#include <QLinearGradient>
#include <QPainter>
#include <QPaintEvent>

namespace darkspark::deck::cards {

using themes::LegacyTheme;

GlassActionButton::GlassActionButton(const QString& text, QWidget* parent)
    : QPushButton(text, parent) {
    setMinimumSize(128, 112);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setCursor(Qt::PointingHandCursor);
    setFlat(true);
}

void GlassActionButton::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const qreal travel = isDown() ? 5.0 : 0.0;
    QRectF key = QRectF(rect()).adjusted(5.0, 4.0 + travel, -5.0, -9.0 + travel);

    // Recess and lower shadow create visible depth against the dashboard.
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, isDown() ? 95 : 180));
    painter.drawRoundedRect(key.translated(0.0, isDown() ? 2.0 : 6.0), 14, 14);

    QColor top = isEnabled() ? QColor(41, 55, 66) : QColor(19, 24, 28);
    QColor bottom = isEnabled() ? QColor(8, 13, 18) : QColor(8, 10, 12);
    if (isDown()) {
        top = QColor(18, 34, 42);
        bottom = QColor(4, 9, 13);
    } else if (underMouse() || hasFocus()) {
        top = QColor(45, 75, 88);
    }

    QLinearGradient glass(key.topLeft(), key.bottomLeft());
    glass.setColorAt(0.0, top);
    glass.setColorAt(0.48, QColor(16, 25, 32));
    glass.setColorAt(1.0, bottom);
    painter.setBrush(glass);
    painter.setPen(QPen(isEnabled() ? LegacyTheme::borderActive()
                                    : LegacyTheme::borderSubtle(),
                        isDown() ? 2.0 : 1.0));
    painter.drawRoundedRect(key, 14, 14);

    // Glass reflection across the upper face.
    QRectF reflection = key.adjusted(4.0, 4.0, -4.0, -key.height() * 0.54);
    QLinearGradient shine(reflection.topLeft(), reflection.bottomLeft());
    shine.setColorAt(0.0, QColor(255, 255, 255, isDown() ? 22 : 72));
    shine.setColorAt(1.0, QColor(255, 255, 255, 0));
    painter.setPen(Qt::NoPen);
    painter.setBrush(shine);
    painter.drawRoundedRect(reflection, 10, 10);

    QFont labelFont = font();
    labelFont.setBold(true);
    labelFont.setPixelSize(LegacyTheme::fontCardSubtitle());
    painter.setFont(labelFont);
    painter.setPen(isEnabled() ? LegacyTheme::textPrimary()
                               : LegacyTheme::textDisabled());
    painter.drawText(key.adjusted(10, 10, -10, -10),
                     Qt::AlignCenter | Qt::TextWordWrap, text());
}

}  // namespace darkspark::deck::cards
