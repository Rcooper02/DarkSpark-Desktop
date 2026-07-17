// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_DECK_CARDS_DASHBOARDCARD_HPP
#define DARKSPARK_DECK_CARDS_DASHBOARDCARD_HPP

#include <QFrame>
#include <QString>

class QLabel;
class QVBoxLayout;
class QGraphicsDropShadowEffect;

namespace darkspark::deck::cards {

class StatusIndicator;

/// Reusable presentation card for Deck Mode.
///
/// A DashboardCard is organized into clear logical regions (docs/
/// VISUAL_LANGUAGE.md page/card anatomy):
///   - header:  title (+ status indicator)
///   - subtitle: optional, quieter, clearly separated
///   - divider:  optional thin separator
///   - content:  meaningful placeholder region
///   - footer:   status text region, consistently aligned at the bottom
///
/// In Deck-1 it remains a pure placeholder: it collects no data, owns no
/// service, and exposes no plugin API. It stays reusable for future CPU, GPU,
/// media, chat, homepage, weather, and notification cards.
///
/// Visual state, accent, and size are exposed as small enums. State is conveyed
/// by a painted StatusIndicator shape AND status text AND color, so meaning
/// never depends on color alone (docs/STYLE_GUIDE.md). Semantic precedence
/// (Error > Warning > Unavailable > Disabled > Loading > Interaction > Accent >
/// Normal) is resolved in refreshVisualState() and expressed through a single
/// "legacyState" property value plus a separate "legacyAccent" property.
///
/// The API is deliberately narrow and concrete; no base classes beyond this one.
///
/// Ownership: a QWidget owned by its Qt parent (typically a DeckPage).
/// Threading: GUI thread only.
class DashboardCard : public QFrame {
    Q_OBJECT

public:
    enum class State {
        Normal,
        Loading,
        Empty,
        Unavailable,
        Warning,
        Error,
        Disabled
    };

    enum class Accent { None, Cyan, Purple };

    /// Presentation size role. A layout hint only; DeckPage interprets it. Not
    /// a layout engine, not user-resizable, not persisted.
    enum class Size { Small, Medium, Large, Wide };

    explicit DashboardCard(QString title, QWidget* parent = nullptr);

    // --- Content ------------------------------------------------------------
    void setTitle(const QString& title);
    void setSubtitle(const QString& subtitle);      ///< empty hides the subtitle
    void setStatusText(const QString& status);      ///< custom footer status text
    /// Optional placeholder line shown in the content region. Empty shows a
    /// default, quiet placeholder so the region still reads as intentional.
    void setPlaceholderText(const QString& text);

    [[nodiscard]] QString title() const;
    [[nodiscard]] QString subtitle() const;
    [[nodiscard]] QString statusText() const;

    // --- State / accent / size ---------------------------------------------
    void setState(State state);
    [[nodiscard]] State state() const;

    void setAccent(Accent accent);
    [[nodiscard]] Accent accent() const;

    void setSizeRole(Size size);
    [[nodiscard]] Size sizeRole() const;

signals:
    /// Emitted on activation by touch/click or keyboard (Space/Return) while not
    /// disabled. Deck-1 wires no behavior to this.
    void activated();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;

private:
    /// Resolve semantic precedence into a single "legacyState" property value
    /// and re-polish so the centralized style sheet re-evaluates.
    void refreshVisualState();
    /// Update the status indicator, default status text, and dividers for the
    /// current content state.
    void applyStateContent();
    /// Enable or disable the controlled cyan focus/press glow.
    void setGlowActive(bool active);

    [[nodiscard]] bool isInteractive() const;

    QLabel* titleLabel_;
    QLabel* subtitleLabel_;
    QLabel* placeholderLabel_;
    QLabel* statusLabel_;
    QFrame* divider_;
    StatusIndicator* indicator_;
    QGraphicsDropShadowEffect* glow_;

    State state_ = State::Normal;
    Accent accent_ = Accent::None;
    Size size_ = Size::Medium;

    bool pressed_ = false;
    bool customStatusText_ = false;
};

}  // namespace darkspark::deck::cards

#endif  // DARKSPARK_DECK_CARDS_DASHBOARDCARD_HPP
