// SPDX-License-Identifier: GPL-3.0-or-later
#include "deck/pages/SelectionOverlay.hpp"

#include <QPainter>
#include <QPaintEvent>
#include <QPen>

#include "themes/LegacyTheme.hpp"

namespace darkspark::deck::pages {

SelectionOverlay::SelectionOverlay(QWidget* parent) : QWidget(parent) {
    // Transparent to mouse events so clicks pass through to the instruments and
    // the page's hit-testing; purely a visual layer.
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    // No background: only the border is drawn.
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    hide();
}

void SelectionOverlay::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    QPen pen(themes::LegacyTheme::accentCyan());
    pen.setWidth(2);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    // Inset by the pen half-width so the 2px stroke stays inside the widget rect.
    const QRectF r = rect().adjusted(1, 1, -1, -1);
    painter.drawRoundedRect(r, 6, 6);
}

}  // namespace darkspark::deck::pages
