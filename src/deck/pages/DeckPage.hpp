// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_DECK_PAGES_DECKPAGE_HPP
#define DARKSPARK_DECK_PAGES_DECKPAGE_HPP

#include <QString>
#include <QWidget>

class QHBoxLayout;

namespace darkspark::deck::cards {
class DashboardCard;
}

namespace darkspark::deck::pages {

/// A single Deck Mode page: a layout container that arranges cards.
///
/// DeckPage owns its title and the layout of its cards. Per the foundation, a
/// page contains no business logic and owns no external data; in Deck-0 it
/// simply holds placeholder DashboardCards. Pages are a Deck presentation
/// concept and are not assumed to be shared with the standard desktop window.
///
/// Ownership: a QWidget; owned by its Qt parent (the PageManager's stack).
/// Added cards are reparented to this page. Threading: GUI thread only.
class DeckPage : public QWidget {
    Q_OBJECT

public:
    explicit DeckPage(QString title, QWidget* parent = nullptr);

    /// Add a card to the page's horizontal card row. The page takes ownership
    /// via Qt parenting.
    void addCard(cards::DashboardCard* card);

    [[nodiscard]] QString title() const;

private:
    QString title_;
    QHBoxLayout* cardRow_;
};

}  // namespace darkspark::deck::pages

#endif  // DARKSPARK_DECK_PAGES_DECKPAGE_HPP
