// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_THEMES_LEGACYTHEME_HPP
#define DARKSPARK_THEMES_LEGACYTHEME_HPP

#include <QColor>
#include <QString>

class QApplication;

namespace darkspark::themes {

/// Centralized definition of the Legacy visual theme.
///
/// LegacyTheme is the single source of truth for DarkSpark Desktop's
/// appearance in Deck-0: colors, spacing, border widths, corner radii, and
/// the derived Qt style sheet. It contains no behavior beyond producing style
/// data; per the project foundation, a theme never alters application logic.
///
/// Values here mirror the tokens documented in docs/design-system.md. Where
/// design-system.md marks a value as "initial / subject to review", the same
/// applies here; token *meaning* is stable, exact values may be tuned.
///
/// Ownership/threading: all members are static and stateless. Safe to call
/// from the GUI thread during startup. Not intended for use off the GUI
/// thread (it touches QApplication style).
class LegacyTheme {
public:
    LegacyTheme() = delete;

    // --- Color tokens (see docs/design-system.md) ---------------------------
    static QColor backgroundBase();     ///< color.background.base
    static QColor backgroundRaised();   ///< color.background.raised
    static QColor backgroundOverlay();  ///< color.background.overlay
    static QColor borderSubtle();       ///< color.border.subtle
    static QColor borderActive();       ///< color.border.active
    static QColor accentCyan();         ///< color.accent.cyan
    static QColor accentPurple();       ///< color.accent.purple
    static QColor textPrimary();        ///< color.text.primary
    static QColor textSecondary();      ///< color.text.secondary
    static QColor textDisabled();       ///< color.text.disabled
    static QColor statusWarning();      ///< color.status.warning
    static QColor statusError();        ///< color.status.error
    static QColor statusGood();         ///< color.status.good

    // --- Metric tokens ------------------------------------------------------
    static int spaceSm();          ///< space.sm  (8)
    static int spaceMd();          ///< space.md  (12)
    static int spaceLg();          ///< space.lg  (16)
    static int spaceXl();          ///< space.xl  (24)
    static int borderHairline();   ///< border.hairline (1)
    static int radiusMd();         ///< radius.md (8)
    static int radiusLg();         ///< radius.lg (12)
    static int touchTargetMin();   ///< touch.target.min (44)

    /// Apply the Legacy theme application-wide.
    ///
    /// Installs the palette and the global style sheet on the given
    /// application. Called once during startup. Passing nullptr is a no-op.
    static void apply(QApplication* app);

    /// The global style sheet string for the Legacy theme.
    ///
    /// Exposed separately so individual widgets/tests can reason about styling
    /// without a running QApplication.
    static QString styleSheet();

    /// Object-name-scoped helpers used by widgets so styling stays centralized
    /// here rather than scattered as inline stylesheets across the UI.
    static QString cardObjectName();       ///< object name a DashboardCard sets
    static QString pageObjectName();       ///< object name a DeckPage sets
    static QString exitButtonObjectName(); ///< object name the Exit control sets
};

}  // namespace darkspark::themes

#endif  // DARKSPARK_THEMES_LEGACYTHEME_HPP
