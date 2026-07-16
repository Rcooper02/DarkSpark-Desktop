// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_DECK_DECKWINDOW_HPP
#define DARKSPARK_DECK_DECKWINDOW_HPP

#include <QWidget>

class QScreen;

namespace darkspark::deck::navigation {
class PageManager;
}

namespace darkspark::deck {

/// The Deck Mode presentation window.
///
/// DeckWindow hosts the PageManager and the five placeholder pages. In Deck
/// Mode it is frameless and shown fullscreen on a chosen screen, sized for the
/// 2560x720 target but functional at other sizes. It always provides a visible
/// Exit control and honors the Escape key so a user can never be trapped in
/// fullscreen.
///
/// This window can also be shown as an ordinary resizable window (used by the
/// standard development window to embed/launch the Deck experience) by not
/// enabling Deck Mode framing.
///
/// Ownership: a top-level QWidget owned by the Application. Threading: GUI
/// thread only.
class DeckWindow : public QWidget {
    Q_OBJECT

public:
    explicit DeckWindow(QWidget* parent = nullptr);

    /// Present as a frameless fullscreen Deck surface on the given screen.
    /// If `screen` is nullptr, the primary screen is used.
    void showDeckFullscreen(QScreen* screen);

    /// Present as a normal resizable window (development framing).
    void showWindowed();

signals:
    /// Emitted when the user requests to leave Deck Mode (Exit button or
    /// Escape). The owner decides what "leaving" means (close, or return to a
    /// desktop window).
    void exitRequested();

protected:
    void keyPressEvent(QKeyEvent* event) override;

private:
    void buildPages();

    navigation::PageManager* pageManager_;
};

}  // namespace darkspark::deck

#endif  // DARKSPARK_DECK_DECKWINDOW_HPP
