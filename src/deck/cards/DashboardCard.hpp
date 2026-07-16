// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_DECK_CARDS_DASHBOARDCARD_HPP
#define DARKSPARK_DECK_CARDS_DASHBOARDCARD_HPP

#include <QFrame>
#include <QString>

class QLabel;

namespace darkspark::deck::cards {

/// Reusable presentation card for Deck Mode.
///
/// A DashboardCard renders a title and an optional status/subtitle line under
/// the Legacy theme. In Deck-0 it is a pure placeholder: it collects no data,
/// owns no service, and exposes no plugin API. It is a presentation component
/// only, consistent with docs/FOUNDATION.md ("cards are reusable presentation
/// components", "UI never collects or owns external data").
///
/// Ownership: a QWidget; owned by its Qt parent (typically a DeckPage).
/// Threading: GUI thread only, like all QWidget subclasses.
class DashboardCard : public QFrame {
    Q_OBJECT

public:
    /// Construct a card with a title and optional placeholder status text.
    explicit DashboardCard(QString title, QString status = QString(),
                           QWidget* parent = nullptr);

    /// Update the card's title text.
    void setTitle(const QString& title);
    /// Update the card's status/subtitle text. Empty hides the status line.
    void setStatus(const QString& status);

    [[nodiscard]] QString title() const;
    [[nodiscard]] QString status() const;

private:
    QLabel* titleLabel_;
    QLabel* statusLabel_;
};

}  // namespace darkspark::deck::cards

#endif  // DARKSPARK_DECK_CARDS_DASHBOARDCARD_HPP
