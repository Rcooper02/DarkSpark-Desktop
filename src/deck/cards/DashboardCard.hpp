// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_DECK_CARDS_DASHBOARDCARD_HPP
#define DARKSPARK_DECK_CARDS_DASHBOARDCARD_HPP

#include <QFrame>
#include <QString>

class QLabel;
class QHBoxLayout;

namespace darkspark::deck::cards {

/// Reusable presentation card for Deck Mode.
///
/// A DashboardCard renders a title, an optional subtitle, and a status line
/// with a small status indicator, styled by the Legacy theme. In Deck-1 it is
/// still a pure placeholder: it collects no data, owns no service, and exposes
/// no plugin API. It is a presentation component only, consistent with
/// docs/FOUNDATION.md ("cards are reusable presentation components", "UI never
/// collects or owns external data").
///
/// Visual state and accent are exposed as small enums and drive centralized
/// styling via dynamic properties; the card carries no inline style sheet.
/// State is always conveyed by a status label and indicator shape in addition
/// to color, so meaning never depends on color alone (docs/STYLE_GUIDE.md).
///
/// The API is deliberately narrow and concrete. No base classes beyond this
/// one are introduced.
///
/// Ownership: a QWidget; owned by its Qt parent (typically a DeckPage).
/// Threading: GUI thread only, like all QWidget subclasses.
class DashboardCard : public QFrame {
    Q_OBJECT

public:
    /// Content/visual state of the card.
    enum class State {
        Normal,       ///< ordinary presentation
        Loading,      ///< data is being prepared (placeholder only in Deck-1)
        Empty,        ///< no content to show (distinct from a false zero)
        Unavailable,  ///< source/integration not present or not reachable
        Warning,      ///< attention needed
        Error,        ///< a failure state
        Disabled      ///< not interactive
    };

    /// Optional accent role for light visual differentiation.
    enum class Accent { None, Cyan, Purple };

    /// Presentation size role. A layout hint only; DeckPage interprets it. Not
    /// a layout engine, not user-resizable, not persisted.
    enum class Size { Small, Medium, Large, Wide };

    explicit DashboardCard(QString title, QWidget* parent = nullptr);

    // --- Content ------------------------------------------------------------
    void setTitle(const QString& title);
    void setSubtitle(const QString& subtitle);  ///< empty hides the subtitle
    void setStatusText(const QString& status);  ///< custom status line text

    [[nodiscard]] QString title() const;
    [[nodiscard]] QString subtitle() const;
    [[nodiscard]] QString statusText() const;

    // --- State / accent / size ---------------------------------------------
    /// Set the visual state. Updates the status indicator and, unless a custom
    /// status text was set, a default human-readable status label for the
    /// state. Also toggles interactivity for the Disabled state.
    void setState(State state);
    [[nodiscard]] State state() const;

    void setAccent(Accent accent);
    [[nodiscard]] Accent accent() const;

    void setSizeRole(Size size);
    [[nodiscard]] Size sizeRole() const;

signals:
    /// Emitted when the card is activated by touch/click or keyboard (Space or
    /// Return) while not disabled. Deck-1 wires no behavior to this; it exists
    /// so pages can respond later without changing the card API.
    void activated();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;

private:
    /// Recompute the styled state property and re-polish so the style sheet
    /// re-evaluates. `pressedOrFocused` transient visuals take precedence over
    /// content state for the border treatment.
    void refreshVisualState();
    /// Update the status indicator glyph and default status text for a state.
    void applyStateContent();
    [[nodiscard]] bool isInteractive() const;

    QLabel* titleLabel_;
    QLabel* subtitleLabel_;
    QLabel* statusLabel_;
    QLabel* statusDot_;

    State state_ = State::Normal;
    Accent accent_ = Accent::None;
    Size size_ = Size::Medium;

    bool pressed_ = false;
    bool customStatusText_ = false;
};

}  // namespace darkspark::deck::cards

#endif  // DARKSPARK_DECK_CARDS_DASHBOARDCARD_HPP
