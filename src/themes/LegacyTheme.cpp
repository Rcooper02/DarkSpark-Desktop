// SPDX-License-Identifier: GPL-3.0-or-later
#include "themes/LegacyTheme.hpp"

#include <QApplication>
#include <QPalette>

namespace darkspark::themes {

// --- Color tokens -----------------------------------------------------------
// Roles per docs/VISUAL_LANGUAGE.md. Deep neutral backgrounds, restrained cyan,
// selective purple, high-contrast primary text, muted supporting text.
QColor LegacyTheme::backgroundBase() { return QColor(0x05, 0x07, 0x0A); }
QColor LegacyTheme::backgroundPage() { return QColor(0x06, 0x08, 0x0C); }
QColor LegacyTheme::backgroundRaised() { return QColor(0x0D, 0x12, 0x18); }
QColor LegacyTheme::backgroundOverlay() { return QColor(0x13, 0x1B, 0x24); }
QColor LegacyTheme::backgroundDisabled() { return QColor(0x08, 0x0B, 0x0F); }
QColor LegacyTheme::borderSubtle() { return QColor(0x18, 0x24, 0x2C); }
QColor LegacyTheme::borderStrong() { return QColor(0x24, 0x33, 0x3D); }
QColor LegacyTheme::borderActive() { return QColor(0x3F, 0xE0, 0xFF); }
QColor LegacyTheme::accentCyan() { return QColor(0x3F, 0xE0, 0xFF); }
QColor LegacyTheme::accentPurple() { return QColor(0x9B, 0x7B, 0xFF); }
QColor LegacyTheme::textPrimary() { return QColor(0xE6, 0xF2, 0xF5); }
QColor LegacyTheme::textSecondary() { return QColor(0x8A, 0x9E, 0xA8); }
QColor LegacyTheme::textDisabled() { return QColor(0x45, 0x54, 0x5C); }
QColor LegacyTheme::statusWarning() { return QColor(0xFF, 0xC2, 0x4B); }
QColor LegacyTheme::statusError() { return QColor(0xFF, 0x5C, 0x6C); }
QColor LegacyTheme::statusGood() { return QColor(0x4B, 0xE3, 0x8A); }

// --- Spacing ----------------------------------------------------------------
int LegacyTheme::spaceXs() { return 4; }
int LegacyTheme::spaceSm() { return 8; }
int LegacyTheme::spaceMd() { return 12; }
int LegacyTheme::spaceLg() { return 16; }
int LegacyTheme::spaceXl() { return 24; }
int LegacyTheme::space2xl() { return 32; }
int LegacyTheme::space3xl() { return 48; }

// --- Border / radius --------------------------------------------------------
int LegacyTheme::borderHairline() { return 1; }
int LegacyTheme::borderThin() { return 2; }
int LegacyTheme::radiusSm() { return 6; }
int LegacyTheme::radiusMd() { return 10; }
int LegacyTheme::radiusLg() { return 12; }
int LegacyTheme::touchTargetMin() { return 44; }
int LegacyTheme::touchTargetPreferred() { return 52; }

// --- Typography (px), docs/VISUAL_LANGUAGE.md scale -------------------------
int LegacyTheme::fontApplicationTitle() { return 28; }
int LegacyTheme::fontPageTitle() { return 26; }
int LegacyTheme::fontPageSubtitle() { return 15; }
int LegacyTheme::fontCardTitle() { return 20; }
int LegacyTheme::fontCardSubtitle() { return 14; }
int LegacyTheme::fontPrimaryValue() { return 34; }
int LegacyTheme::fontSupporting() { return 13; }
int LegacyTheme::fontStatus() { return 12; }
int LegacyTheme::fontAnnotation() { return 11; }

// --- Motion -----------------------------------------------------------------
int LegacyTheme::motionFast() { return 120; }
int LegacyTheme::motionStandard() { return 220; }

// --- Glow -------------------------------------------------------------------
int LegacyTheme::glowRadius() { return 16; }

// --- Object names -----------------------------------------------------------
QString LegacyTheme::cardObjectName() { return QStringLiteral("LegacyCard"); }
QString LegacyTheme::cardHeaderObjectName() { return QStringLiteral("LegacyCardHeader"); }
QString LegacyTheme::cardContentObjectName() { return QStringLiteral("LegacyCardContent"); }
QString LegacyTheme::cardDividerObjectName() { return QStringLiteral("LegacyCardDivider"); }
QString LegacyTheme::pageObjectName() { return QStringLiteral("LegacyPage"); }
QString LegacyTheme::pageHeaderObjectName() { return QStringLiteral("LegacyPageHeader"); }
QString LegacyTheme::exitButtonObjectName() { return QStringLiteral("LegacyExitButton"); }

QString LegacyTheme::styleSheet() {
    // Single centralized style sheet. Selectors are scoped by object name and
    // dynamic properties (legacyRole, legacyState, legacyAccent). No inline
    // per-widget stylesheets exist elsewhere. No external fonts or icons.
    //
    // Semantic precedence (docs/VISUAL_LANGUAGE.md + batch spec):
    //   Error > Warning > Unavailable > Disabled > Loading > Interaction >
    //   Accent > Normal.
    // The card sets exactly one winning value into the "legacyState" property
    // (see DashboardCard::refreshVisualState), so precedence is resolved in C++
    // and the sheet only needs one selector per resolved value. Accent is a
    // separate property applied only when no state/interaction overrides it.
    const QString base = backgroundBase().name();
    const QString page = backgroundPage().name();
    const QString raised = backgroundRaised().name();
    const QString overlay = backgroundOverlay().name();
    const QString disabledSurface = backgroundDisabled().name();
    const QString subtle = borderSubtle().name();
    const QString strong = borderStrong().name();
    const QString active = borderActive().name();
    const QString cyan = accentCyan().name();
    const QString purple = accentPurple().name();
    const QString textPri = textPrimary().name();
    const QString textSec = textSecondary().name();
    const QString textDis = textDisabled().name();
    const QString warn = statusWarning().name();
    const QString err = statusError().name();

    QString sheet;

    // Base widget + page surfaces.
    sheet += QStringLiteral(
                 "QWidget { background-color: %1; color: %2; font-size: %3px; }"
                 "QWidget#%4 { background-color: %5; }")
                 .arg(base, textPri)
                 .arg(fontSupporting())
                 .arg(pageObjectName(), page);

    // Card: slightly elevated surface, restrained idle border (border.strong is
    // used for separation but kept low intensity), rounded corners. Idle cards
    // do NOT glow (calm when idle).
    sheet += QStringLiteral(
                 "QFrame#%1 { background-color: %2; border: %3px solid %4;"
                 " border-radius: %5px; }")
                 .arg(cardObjectName(), raised)
                 .arg(borderHairline())
                 .arg(strong)
                 .arg(radiusMd());  // cards: 10px (docs/VISUAL_LANGUAGE.md)

    // Accent (lowest precedence above Normal): a quiet tint on the idle border.
    // Applied only when legacyState == "normal" so state/interaction always win.
    sheet += QStringLiteral(
                 "QFrame#%1[legacyState=\"normal\"][legacyAccent=\"cyan\"]"
                 " { border-color: %2; }"
                 "QFrame#%1[legacyState=\"normal\"][legacyAccent=\"purple\"]"
                 " { border-color: %3; }")
                 .arg(cardObjectName(), cyan, purple);

    // Interaction: focus and pressed keep the SAME border width as idle
    // (borderHairline) to guarantee no layout shift (docs/VISUAL_LANGUAGE.md
    // "no large layout shift"). Emphasis comes from a brighter cyan border
    // color plus a controlled cyan glow applied as a QGraphicsDropShadowEffect
    // in DashboardCard (style sheets cannot render glow). Pressed additionally
    // uses a subtle background illumination.
    sheet += QStringLiteral(
                 "QFrame#%1[legacyState=\"focused\"]"
                 " { border: %2px solid %3; background-color: %4; }"
                 "QFrame#%1[legacyState=\"pressed\"]"
                 " { border: %2px solid %3; background-color: %5; }")
                 .arg(cardObjectName())
                 .arg(borderHairline())
                 .arg(active, raised, overlay);

    // Content states (higher precedence). Only one is ever active at a time.
    sheet += QStringLiteral(
                 "QFrame#%1[legacyState=\"loading\"]"
                 " { border: %2px solid %3; }"
                 "QFrame#%1[legacyState=\"empty\"]"
                 " { border: %2px solid %4; }"
                 "QFrame#%1[legacyState=\"unavailable\"]"
                 " { border: %2px dashed %5; }"
                 "QFrame#%1[legacyState=\"disabled\"]"
                 " { border: %2px solid %4; background-color: %6; }"
                 "QFrame#%1[legacyState=\"warning\"]"
                 " { border: %7px solid %8; }"
                 "QFrame#%1[legacyState=\"error\"]"
                 " { border: %7px solid %9; }")
                 .arg(cardObjectName())
                 .arg(borderHairline())
                 .arg(subtle)         // loading: quiet
                 .arg(strong)         // empty: structural
                 .arg(textDis)        // unavailable: dashed muted
                 .arg(disabledSurface)
                 .arg(borderThin())
                 .arg(warn, err);

    // Optional divider between header and content.
    sheet += QStringLiteral(
                 "QFrame#%1 { background-color: %2; border: none;"
                 " max-height: %3px; min-height: %3px; }")
                 .arg(cardDividerObjectName(), subtle)
                 .arg(borderHairline());

    // Header/content containers are transparent so the card surface shows.
    sheet += QStringLiteral(
                 "QWidget#%1, QWidget#%2 { background-color: transparent; }")
                 .arg(cardHeaderObjectName(), cardContentObjectName());

    // Application title: the strongest identity treatment, used sparingly for
    // application identity (e.g. the Desktop window), distinct from pageTitle.
    // Kept in its own block to avoid multi-digit style-arg placeholders.
    sheet += QStringLiteral(
                 "QLabel[legacyRole=\"applicationTitle\"]"
                 " { color: %1; font-size: %2px; font-weight: 700;"
                 " letter-spacing: 1px; }")
                 .arg(textPri)
                 .arg(fontApplicationTitle());

    // Text roles. Weight provides hierarchy before color (VISUAL_LANGUAGE.md).
    sheet += QStringLiteral(
                 "QLabel[legacyRole=\"pageTitle\"]"
                 " { color: %1; font-size: %2px; font-weight: 700;"
                 " letter-spacing: 1px; }"
                 "QLabel[legacyRole=\"pageSubtitle\"]"
                 " { color: %3; font-size: %4px; font-weight: 400; }"
                 "QLabel[legacyRole=\"cardTitle\"]"
                 " { color: %5; font-size: %6px; font-weight: 600; }"
                 "QLabel[legacyRole=\"cardSubtitle\"]"
                 " { color: %3; font-size: %7px; font-weight: 400; }"
                 "QLabel[legacyRole=\"placeholder\"]"
                 " { color: %3; font-size: %8px; font-weight: 400; }"
                 "QLabel[legacyRole=\"statusText\"]"
                 " { color: %3; font-size: %9px; font-weight: 400; }")
                 .arg(textPri)
                 .arg(fontPageTitle())
                 .arg(textSec)
                 .arg(fontPageSubtitle())
                 .arg(textPri)
                 .arg(fontCardTitle())
                 .arg(fontCardSubtitle())
                 .arg(fontSupporting())
                 .arg(fontStatus());

    // Status text escalates in color only for warning/error (state over accent;
    // color reinforces, never sole signal).
    sheet += QStringLiteral(
                 "QLabel[legacyRole=\"statusText\"][legacyState=\"warning\"]"
                 " { color: %1; font-weight: 600; }"
                 "QLabel[legacyRole=\"statusText\"][legacyState=\"error\"]"
                 " { color: %2; font-weight: 600; }"
                 "QLabel[legacyRole=\"cardTitle\"][legacyState=\"disabled\"]"
                 " { color: %3; }"
                 "QLabel[legacyRole=\"cardSubtitle\"][legacyState=\"disabled\"]"
                 " { color: %3; }")
                 .arg(warn, err, textDis);

    // Buttons (Exit control). Touch-sized; cyan on focus; red intent on the
    // exit control hover/focus.
    sheet += QStringLiteral(
                 "QPushButton { background-color: %1; color: %2; border: %3px solid %4;"
                 " border-radius: %5px; padding: 10px 18px; min-height: %6px; }"
                 "QPushButton:hover, QPushButton:focus { border-color: %7; color: %7; }"
                 "QPushButton:pressed { border-color: %8; }"
                 "QPushButton:disabled { color: %9; border-color: %4; }"
                 "QPushButton#%10:hover, QPushButton#%10:focus { border-color: %11; color: %11; }")
                 .arg(overlay, textPri)
                 .arg(borderHairline())
                 .arg(strong)
                 .arg(radiusSm())  // small controls: 6px
                 .arg(touchTargetMin())
                 .arg(cyan, purple, textDis)
                 .arg(exitButtonObjectName(), err);

    return sheet;
}

void LegacyTheme::apply(QApplication* app) {
    if (app == nullptr) {
        return;
    }

    QPalette palette;
    palette.setColor(QPalette::Window, backgroundBase());
    palette.setColor(QPalette::WindowText, textPrimary());
    palette.setColor(QPalette::Base, backgroundRaised());
    palette.setColor(QPalette::AlternateBase, backgroundOverlay());
    palette.setColor(QPalette::Text, textPrimary());
    palette.setColor(QPalette::Button, backgroundOverlay());
    palette.setColor(QPalette::ButtonText, textPrimary());
    palette.setColor(QPalette::Highlight, accentCyan());
    palette.setColor(QPalette::HighlightedText, backgroundBase());
    palette.setColor(QPalette::Disabled, QPalette::Text, textDisabled());
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, textDisabled());
    app->setPalette(palette);
    app->setStyleSheet(styleSheet());
}

}  // namespace darkspark::themes
