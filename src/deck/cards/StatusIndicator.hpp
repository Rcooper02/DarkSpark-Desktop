// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_DECK_CARDS_STATUSINDICATOR_HPP
#define DARKSPARK_DECK_CARDS_STATUSINDICATOR_HPP

#include <QWidget>

class QTimer;

namespace darkspark::deck::cards {

/// A small custom-painted status indicator.
///
/// StatusIndicator draws a compact shape with QPainter for each card state,
/// replacing the earlier Unicode-glyph approach so rendering no longer depends
/// on system font glyph coverage. It uses no image assets and no icon library
/// and is deliberately not a general icon framework: it paints one of a fixed,
/// closed set of state shapes and nothing else.
///
/// Color reinforces the shape but is never the sole signal; the DashboardCard
/// always shows accompanying status text. Distinct SHAPES (not just colors)
/// separate the states: filled dot, dashed ring (loading), hollow ring (empty),
/// horizontal bar (unavailable), triangle (warning), cross (error), muted
/// hollow ring (disabled).
///
/// The Loading state uses a restrained rotating arc. The animation runs only
/// while the widget is visible and enabled and stops otherwise, so hidden or
/// disabled indicators do no work.
///
/// Ownership: a QWidget owned by its Qt parent (a DashboardCard). Threading:
/// GUI thread only.
class StatusIndicator : public QWidget {
    Q_OBJECT

public:
    /// Mirrors DashboardCard::State. Independent so the widget has no dependency
    /// on DashboardCard; the card maps between the two enums.
    enum class State {
        Normal,
        Loading,
        Empty,
        Unavailable,
        Warning,
        Error,
        Disabled
    };

    explicit StatusIndicator(QWidget* parent = nullptr);

    void setState(State state);
    [[nodiscard]] State state() const;

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    void updateAnimationState();
    [[nodiscard]] QColor colorForState() const;

    State state_ = State::Normal;
    QTimer* spinTimer_;
    int spinAngle_ = 0;  ///< degrees, advanced while Loading
};

}  // namespace darkspark::deck::cards

#endif  // DARKSPARK_DECK_CARDS_STATUSINDICATOR_HPP
