// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_DECK_PAGES_DECKPAGE_HPP
#define DARKSPARK_DECK_PAGES_DECKPAGE_HPP

#include <QString>
#include <QVector>
#include <QWidget>

class QGridLayout;

namespace darkspark::deck::cards {
class DashboardCard;
}

namespace darkspark::deck::pages {

/// A single Deck Mode page: a responsive layout container that arranges cards.
///
/// DeckPage owns its title and the arrangement of its cards. Per the
/// foundation, a page contains no business logic and owns no external data; in
/// Deck-1 it holds placeholder DashboardCards. Pages are a Deck presentation
/// concept and are not assumed to be shared with the standard desktop window.
///
/// Cards are placed in a grid whose column count adapts to the available width
/// (wide at 2560x720, fewer columns in a small development window). A card's
/// Size role is a hint that maps to a column span. This is a responsive layout
/// only: no drag-and-drop, no user editing, no persistence, no layout engine.
///
/// Ownership: a QWidget; owned by its Qt parent (the PageManager's stack).
/// Added cards are reparented to this page. Threading: GUI thread only.
class DeckPage : public QWidget {
    Q_OBJECT

public:
    explicit DeckPage(QString title, QString subtitle = QString(),
                      QWidget* parent = nullptr);

    /// Add a card. The page takes ownership via Qt parenting and places it
    /// according to the current column count and the card's size role.
    void addCard(cards::DashboardCard* card);

    [[nodiscard]] QString title() const;
    [[nodiscard]] QString subtitle() const;
    [[nodiscard]] int cardCount() const;

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    /// Column count that fits the current content width, bounded to a sensible
    /// range. Derived from a target card width; no hard-coded pixel positions.
    [[nodiscard]] int columnsForWidth(int contentWidth) const;
    /// Re-place all cards into the grid using the given column count.
    void relayout(int columns);
    /// Column span for a card's size role, clamped to the column count.
    [[nodiscard]] static int spanForSize(const cards::DashboardCard* card, int columns);

    QString title_;
    QString subtitle_;
    QGridLayout* grid_;
    QVector<cards::DashboardCard*> cards_;
    int currentColumns_ = 0;
};

}  // namespace darkspark::deck::pages

#endif  // DARKSPARK_DECK_PAGES_DECKPAGE_HPP
