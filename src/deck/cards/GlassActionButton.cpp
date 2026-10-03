// SPDX-License-Identifier: GPL-3.0-or-later
#include "deck/cards/GlassActionButton.hpp"

#include "themes/LegacyTheme.hpp"

#include <QFileInfo>
#include <QApplication>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QMovie>
#include <QPainter>
#include <QPaintEvent>
#include <QTimer>

namespace darkspark::deck::cards {

using themes::LegacyTheme;

GlassActionButton::GlassActionButton(
    const QString& text,
    QWidget* parent)
    : QPushButton(text, parent),
      longPressTimer_(new QTimer(this)) {

    setMinimumSize(128, 112);
    setSizePolicy(
        QSizePolicy::Expanding,
        QSizePolicy::Expanding);

    setCursor(Qt::PointingHandCursor);
    setFlat(true);
    setAcceptDrops(true);

    longPressTimer_->setSingleShot(true);
    longPressTimer_->setInterval(700);

    connect(
        longPressTimer_,
        &QTimer::timeout,
        this,
        [this]() {
            if (!isDown())
                return;

            longPressTriggered_ = true;
            emit longPressed();
        });
}

void GlassActionButton::clearDeckIcon() {
    staticIcon_ = QIcon();
    iconSource_.clear();

    if (movie_ != nullptr) {
        movie_->stop();
        movie_->deleteLater();
        movie_ = nullptr;
    }

    update();
}

void GlassActionButton::setDeckIcon(
    const QString& source) {

    clearDeckIcon();

    const QString trimmed = source.trimmed();

    if (trimmed.isEmpty())
        return;

    iconSource_ = trimmed;

    const QFileInfo info(trimmed);

    if (info.exists() && info.isFile()) {
        const QString suffix =
            info.suffix().toLower();

        // Try animated formats first.
        if (suffix == QStringLiteral("gif") ||
            suffix == QStringLiteral("webp")) {

            auto* candidate = new QMovie(trimmed);

            if (candidate->isValid()) {
                movie_ = candidate;
                movie_->setParent(this);
                movie_->setCacheMode(QMovie::CacheAll);

                connect(
                    movie_,
                    &QMovie::frameChanged,
                    this,
                    [this](int) {
                        update();
                    });

                movie_->start();
                update();
                return;
            }

            delete candidate;
        }

        // Static filesystem image, including SVG/PNG/JPG
        staticIcon_ = QIcon(trimmed);

        if (!staticIcon_.isNull()) {
            update();
            return;
        }
    }

    // If it isn't a path, treat it as a desktop/theme icon name.
    staticIcon_ = QIcon::fromTheme(trimmed);

    update();
}

void GlassActionButton::mousePressEvent(
    QMouseEvent* event) {

    longPressTriggered_ = false;

    if (event->button() == Qt::LeftButton) {
        pressPosition_ = event->position().toPoint();
        longPressTimer_->start();
    }

    QPushButton::mousePressEvent(event);
}

void GlassActionButton::mouseMoveEvent(
    QMouseEvent* event) {

    if (!(event->buttons() & Qt::LeftButton) ||
        deckSlot_ < 0) {
        QPushButton::mouseMoveEvent(event);
        return;
    }

    const int distance =
        (event->position().toPoint() -
         pressPosition_).manhattanLength();

    if (distance <
        QApplication::startDragDistance()) {
        QPushButton::mouseMoveEvent(event);
        return;
    }

    longPressTimer_->stop();
    longPressTriggered_ = false;

    auto* mime = new QMimeData();
    mime->setData(
        QStringLiteral(
            "application/x-darkspark-deck-slot"),
        QByteArray::number(deckSlot_));

    auto* drag = new QDrag(this);
    drag->setMimeData(mime);

    const QPixmap preview = grab();

    if (!preview.isNull()) {
        drag->setPixmap(preview);
        drag->setHotSpot(
            event->position().toPoint());
    }

    setDown(false);
    drag->exec(Qt::MoveAction);
}

void GlassActionButton::mouseReleaseEvent(
    QMouseEvent* event) {

    longPressTimer_->stop();

    if (longPressTriggered_) {
        longPressTriggered_ = false;
        setDown(false);
        event->accept();
        return;
    }

    QPushButton::mouseReleaseEvent(event);
}

void GlassActionButton::leaveEvent(QEvent* event) {
    longPressTimer_->stop();
    QPushButton::leaveEvent(event);
}

void GlassActionButton::dragEnterEvent(
    QDragEnterEvent* event) {

    if (!event->mimeData()->hasFormat(
            QStringLiteral(
                "application/x-darkspark-deck-slot"))) {
        return;
    }

    bool ok = false;

    const int source =
        event->mimeData()
            ->data(QStringLiteral(
                "application/x-darkspark-deck-slot"))
            .toInt(&ok);

    if (ok && source != deckSlot_)
        event->acceptProposedAction();
}

void GlassActionButton::dropEvent(
    QDropEvent* event) {

    bool ok = false;

    const int source =
        event->mimeData()
            ->data(QStringLiteral(
                "application/x-darkspark-deck-slot"))
            .toInt(&ok);

    if (!ok ||
        source < 0 ||
        deckSlot_ < 0 ||
        source == deckSlot_) {
        return;
    }

    emit slotDropped(source, deckSlot_);
    event->acceptProposedAction();
}

void GlassActionButton::paintEvent(
    QPaintEvent* event) {

    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(
        QPainter::Antialiasing,
        true);

    painter.setRenderHint(
        QPainter::SmoothPixmapTransform,
        true);

    const qreal travel =
        isDown() ? 5.0 : 0.0;

    QRectF key =
        QRectF(rect()).adjusted(
            5.0,
            4.0 + travel,
            -5.0,
            -9.0 + travel);

    // Recess and lower shadow.
    painter.setPen(Qt::NoPen);
    painter.setBrush(
        QColor(
            0,
            0,
            0,
            isDown() ? 95 : 180));

    painter.drawRoundedRect(
        key.translated(
            0.0,
            isDown() ? 2.0 : 6.0),
        14,
        14);

    QColor top =
        isEnabled()
            ? QColor(41, 55, 66)
            : QColor(19, 24, 28);

    QColor bottom =
        isEnabled()
            ? QColor(8, 13, 18)
            : QColor(8, 10, 12);

    if (isDown()) {
        top = QColor(18, 34, 42);
        bottom = QColor(4, 9, 13);
    } else if (underMouse() || hasFocus()) {
        top = QColor(45, 75, 88);
    }

    QLinearGradient glass(
        key.topLeft(),
        key.bottomLeft());

    glass.setColorAt(0.0, top);
    glass.setColorAt(
        0.48,
        QColor(16, 25, 32));

    glass.setColorAt(1.0, bottom);

    painter.setBrush(glass);

    painter.setPen(
        QPen(
            isEnabled()
                ? LegacyTheme::borderActive()
                : LegacyTheme::borderSubtle(),
            isDown() ? 2.0 : 1.0));

    painter.drawRoundedRect(
        key,
        14,
        14);

    // Glass reflection.
    QRectF reflection =
        key.adjusted(
            4.0,
            4.0,
            -4.0,
            -key.height() * 0.54);

    QLinearGradient shine(
        reflection.topLeft(),
        reflection.bottomLeft());

    shine.setColorAt(
        0.0,
        QColor(
            255,
            255,
            255,
            isDown() ? 22 : 72));

    shine.setColorAt(
        1.0,
        QColor(255, 255, 255, 0));

    painter.setPen(Qt::NoPen);
    painter.setBrush(shine);

    painter.drawRoundedRect(
        reflection,
        10,
        10);

    const bool hasAnimatedIcon =
        movie_ != nullptr &&
        movie_->isValid();

    const bool hasStaticIcon =
        !staticIcon_.isNull();

    const bool hasIcon =
        hasAnimatedIcon ||
        hasStaticIcon;

    if (hasIcon) {
        // Leave lower portion for label.
        QRect iconArea =
            key.adjusted(
                12,
                9,
                -12,
                -30)
            .toRect();

        QPixmap pixmap;

        if (hasAnimatedIcon) {
            pixmap =
                movie_->currentPixmap();
        } else {
            pixmap =
                staticIcon_.pixmap(
                    iconArea.size(),
                    devicePixelRatio());
        }

        if (!pixmap.isNull()) {
            const QSize targetSize =
                pixmap.size().scaled(
                    iconArea.size(),
                    Qt::KeepAspectRatio);

            const QRect targetRect(
                iconArea.center().x()
                    - targetSize.width() / 2,
                iconArea.center().y()
                    - targetSize.height() / 2,
                targetSize.width(),
                targetSize.height());

            painter.drawPixmap(
                targetRect,
                pixmap);
        }

        QFont labelFont = font();
        labelFont.setBold(true);
        labelFont.setPixelSize(
            qMax(
                10,
                LegacyTheme::fontCardSubtitle() - 2));

        painter.setFont(labelFont);

        painter.setPen(
            isEnabled()
                ? LegacyTheme::textPrimary()
                : LegacyTheme::textDisabled());

        QRectF labelArea(
            key.left() + 6,
            key.bottom() - 28,
            key.width() - 12,
            22);

        painter.drawText(
            labelArea,
            Qt::AlignCenter |
                Qt::TextWordWrap,
            text());

        return;
    }

    // Text-only fallback.
    QFont labelFont = font();
    labelFont.setBold(true);
    labelFont.setPixelSize(
        LegacyTheme::fontCardSubtitle());

    painter.setFont(labelFont);

    painter.setPen(
        isEnabled()
            ? LegacyTheme::textPrimary()
            : LegacyTheme::textDisabled());

    painter.drawText(
        key.adjusted(10, 10, -10, -10),
        Qt::AlignCenter |
            Qt::TextWordWrap,
        text());
}

}  // namespace darkspark::deck::cards
