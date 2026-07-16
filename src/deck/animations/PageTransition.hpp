// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_DECK_ANIMATIONS_PAGETRANSITION_HPP
#define DARKSPARK_DECK_ANIMATIONS_PAGETRANSITION_HPP

#include <QObject>

class QStackedWidget;

namespace darkspark::deck::animations {

/// Encapsulates the page-change transition for a QStackedWidget.
///
/// This keeps transition behavior out of DeckPage and PageManager's core
/// state logic (per the Deck-0 requirement that transition logic be separate
/// from DeckPage). In Deck-0 the transition is a subtle horizontal slide; if
/// animations are unavailable or a transition is already running, it falls
/// back to an instant switch so navigation never blocks or breaks.
///
/// Ownership: a QObject owned by its parent (the PageManager). It does not own
/// the stack it animates. Threading: GUI thread only.
class PageTransition : public QObject {
    Q_OBJECT

public:
    enum class Direction { Forward, Backward };

    explicit PageTransition(QStackedWidget* stack, QObject* parent = nullptr);

    /// Transition the stack to `toIndex`, sliding in the given direction.
    /// If a transition is in progress or the index is out of range, the switch
    /// is applied instantly and safely.
    void transitionTo(int toIndex, Direction direction);

    [[nodiscard]] bool isRunning() const;

signals:
    /// Emitted once the visible page has settled on `index`.
    void settled(int index);

private:
    void finishInstantly(int toIndex);

    QStackedWidget* stack_;
    bool running_ = false;
};

}  // namespace darkspark::deck::animations

#endif  // DARKSPARK_DECK_ANIMATIONS_PAGETRANSITION_HPP
