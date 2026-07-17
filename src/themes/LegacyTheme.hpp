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
/// appearance: colors, spacing, border widths, corner radii, typography sizes,
/// motion durations, and the derived global style sheet. It contains no
/// behavior beyond producing style data; per the project foundation, a theme
/// never alters application logic.
///
/// Values here mirror the tokens documented in docs/design-system.md. Where
/// design-system.md marks a value as "initial / subject to review", the same
/// applies here; token meaning is stable, exact values may be tuned.
///
/// Styling strategy: widgets set an object name and dynamic properties
/// (legacyRole, legacyState, legacyAccent) and the single style sheet returned
/// by styleSheet() targets them. Widgets must not carry inline style sheets so
/// appearance stays centralized here.
///
/// Ownership/threading: all members are static and stateless. Intended for the
/// GUI thread during startup and styling.
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
    static int spaceXs();          ///< space.xs  (4)
    static int spaceSm();          ///< space.sm  (8)
    static int spaceMd();          ///< space.md  (12)
    static int spaceLg();          ///< space.lg  (16)
    static int spaceXl();          ///< space.xl  (24)
    static int spaceXxl();         ///< space.xxl (32)
    static int borderHairline();   ///< border.hairline (1)
    static int borderThin();       ///< border.thin (2)
    static int radiusSm();         ///< radius.sm (4)
    static int radiusMd();         ///< radius.md (8)
    static int radiusLg();         ///< radius.lg (12)
    static int touchTargetMin();   ///< touch.target.min (44)

    // --- Typography sizes (px) ---------------------------------------------
    static int fontSizeTitle();     ///< type.title    (22)
    static int fontSizeSubtitle();  ///< type.subtitle (18)
    static int fontSizeBody();      ///< type.body     (15)
    static int fontSizeCaption();   ///< type.caption  (13)

    // --- Motion (ms) --------------------------------------------------------
    static int motionFast();       ///< motion.fast (120)
    static int motionStandard();   ///< motion.standard (220)

    /// Apply the Legacy theme application-wide (palette + global style sheet).
    /// Passing nullptr is a no-op.
    static void apply(QApplication* app);

    /// The global style sheet string for the Legacy theme.
    static QString styleSheet();

    // --- Object names used by widgets --------------------------------------
    static QString cardObjectName();       ///< object name a DashboardCard sets
    static QString pageObjectName();       ///< object name a DeckPage sets
    static QString exitButtonObjectName(); ///< object name the Exit control sets
    static QString statusDotObjectName();  ///< object name the status indicator sets
};

}  // namespace darkspark::themes

#endif  // DARKSPARK_THEMES_LEGACYTHEME_HPP
