// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_DECK_CARDS_GLASSACTIONBUTTON_HPP
#define DARKSPARK_DECK_CARDS_GLASSACTIONBUTTON_HPP

#include <QIcon>
#include <QPoint>
#include <QPushButton>

class QEvent;
class QDragEnterEvent;
class QDropEvent;
class QMouseEvent;
class QMovie;
class QPaintEvent;
class QTimer;

namespace darkspark::deck::cards {

/// Touch button painted as a raised glass key with optional static/animated icon.
class GlassActionButton final : public QPushButton {
    Q_OBJECT

public:
    explicit GlassActionButton(
        const QString& text,
        QWidget* parent = nullptr);

    /// Accepts either:
    /// - filesystem image path
    /// - Qt theme icon name
    ///
    /// Animated GIF/WebP files use QMovie when supported by Qt.
    void setDeckIcon(const QString& source);
    void setDeckSlot(int slot) { deckSlot_ = slot; }

signals:
    void longPressed();
    void slotDropped(int fromSlot, int toSlot);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    void clearDeckIcon();

    QTimer* longPressTimer_;
    bool longPressTriggered_ = false;
    int deckSlot_ = -1;
    QPoint pressPosition_;

    QString iconSource_;
    QIcon staticIcon_;
    QMovie* movie_ = nullptr;
};

}  // namespace darkspark::deck::cards

#endif  // DARKSPARK_DECK_CARDS_GLASSACTIONBUTTON_HPP
