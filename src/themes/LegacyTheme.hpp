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
/// Token values follow docs/VISUAL_LANGUAGE.md (authoritative) for typography
/// scale, spacing, color roles, border restraint, and controlled glow. Where
/// VISUAL_LANGUAGE.md gives a recommended value, that value is used here.
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

    // --- Color tokens (docs/VISUAL_LANGUAGE.md color roles) -----------------
    static QColor backgroundBase();     ///< application background (deepest)
    static QColor backgroundPage();     ///< page background (~= application)
    static QColor backgroundRaised();   ///< card background (slightly elevated)
    static QColor backgroundOverlay();  ///< interactive overlay (subtle tint)
    static QColor backgroundDisabled(); ///< disabled surface (reduced contrast)
    static QColor borderSubtle();       ///< thin structural border (idle)
    static QColor borderStrong();       ///< card separation border
    static QColor borderActive();       ///< focus/active illuminated border
    static QColor accentCyan();         ///< primary accent
    static QColor accentPurple();       ///< secondary accent
    static QColor textPrimary();        ///< primary information (white-ish)
    static QColor textSecondary();      ///< muted blue-grey supporting text
    static QColor textDisabled();       ///< disabled text
    static QColor statusWarning();      ///< amber
    static QColor statusError();        ///< red
    static QColor statusGood();         ///< green

    // --- Spacing tokens (docs/VISUAL_LANGUAGE.md spacing system) -----------
    static int spaceXs();   ///< 4
    static int spaceSm();   ///< 8
    static int spaceMd();   ///< 12
    static int spaceLg();   ///< 16
    static int spaceXl();   ///< 24
    static int space2xl();  ///< 32
    static int space3xl();  ///< 48

    // --- Border / radius ----------------------------------------------------
    static int borderHairline();   ///< 1
    static int borderThin();       ///< 2
    static int radiusSm();         ///< small controls (6)
    static int radiusMd();         ///< cards (10)
    static int radiusLg();         ///< large surfaces (12)
    static int touchTargetMin();   ///< 44

    // --- Typography sizes (px), docs/VISUAL_LANGUAGE.md typography scale ----
    static int fontApplicationTitle();  ///< 28
    static int fontPageTitle();         ///< 26
    static int fontPageSubtitle();      ///< 15
    static int fontCardTitle();         ///< 20
    static int fontCardSubtitle();      ///< 14
    static int fontPrimaryValue();      ///< 34
    static int fontSupporting();        ///< 13
    static int fontStatus();            ///< 12
    static int fontAnnotation();        ///< 11

    // --- Motion (ms) --------------------------------------------------------
    static int motionFast();       ///< 120
    static int motionStandard();   ///< 220

    // --- Glow ---------------------------------------------------------------
    /// Controlled focus/selection glow blur radius (docs/VISUAL_LANGUAGE.md:
    /// glow is an accent for focus/active navigation, not a background effect).
    static int glowRadius();       ///< 16

    /// Apply the Legacy theme application-wide (palette + global style sheet).
    /// Passing nullptr is a no-op.
    static void apply(QApplication* app);

    /// The global style sheet string for the Legacy theme.
    static QString styleSheet();

    // --- Object names used by widgets --------------------------------------
    static QString cardObjectName();
    static QString cardHeaderObjectName();
    static QString cardContentObjectName();
    static QString cardDividerObjectName();
    static QString pageObjectName();
    static QString pageHeaderObjectName();
    static QString exitButtonObjectName();
};

}  // namespace darkspark::themes

#endif  // DARKSPARK_THEMES_LEGACYTHEME_HPP
