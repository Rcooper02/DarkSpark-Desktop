// SPDX-License-Identifier: GPL-3.0-or-later
#include "themes/LegacyTheme.hpp"

#include <QApplication>
#include <QPalette>

namespace darkspark::themes {

// --- Color tokens -----------------------------------------------------------
// Values mirror docs/design-system.md. Kept as literals in one place so the
// theme remains the single source of truth.
QColor LegacyTheme::backgroundBase() { return QColor(0x05, 0x07, 0x0A); }
QColor LegacyTheme::backgroundRaised() { return QColor(0x0B, 0x0F, 0x14); }
QColor LegacyTheme::backgroundOverlay() { return QColor(0x11, 0x17, 0x21); }
QColor LegacyTheme::borderSubtle() { return QColor(0x1C, 0x2A, 0x33); }
QColor LegacyTheme::borderActive() { return QColor(0x3F, 0xE0, 0xFF); }
QColor LegacyTheme::accentCyan() { return QColor(0x3F, 0xE0, 0xFF); }
QColor LegacyTheme::accentPurple() { return QColor(0x9B, 0x7B, 0xFF); }
QColor LegacyTheme::textPrimary() { return QColor(0xE6, 0xF2, 0xF5); }
QColor LegacyTheme::textSecondary() { return QColor(0x9B, 0xB0, 0xB8); }
QColor LegacyTheme::textDisabled() { return QColor(0x4A, 0x5A, 0x62); }
QColor LegacyTheme::statusWarning() { return QColor(0xFF, 0xC2, 0x4B); }
QColor LegacyTheme::statusError() { return QColor(0xFF, 0x5C, 0x6C); }
QColor LegacyTheme::statusGood() { return QColor(0x4B, 0xE3, 0x8A); }

// --- Metric tokens ----------------------------------------------------------
int LegacyTheme::spaceXs() { return 4; }
int LegacyTheme::spaceSm() { return 8; }
int LegacyTheme::spaceMd() { return 12; }
int LegacyTheme::spaceLg() { return 16; }
int LegacyTheme::spaceXl() { return 24; }
int LegacyTheme::spaceXxl() { return 32; }
int LegacyTheme::borderHairline() { return 1; }
int LegacyTheme::borderThin() { return 2; }
int LegacyTheme::radiusSm() { return 4; }
int LegacyTheme::radiusMd() { return 8; }
int LegacyTheme::radiusLg() { return 12; }
int LegacyTheme::touchTargetMin() { return 44; }

// --- Typography -------------------------------------------------------------
int LegacyTheme::fontSizeTitle() { return 22; }
int LegacyTheme::fontSizeSubtitle() { return 18; }
int LegacyTheme::fontSizeBody() { return 15; }
int LegacyTheme::fontSizeCaption() { return 13; }

// --- Motion -----------------------------------------------------------------
int LegacyTheme::motionFast() { return 120; }
int LegacyTheme::motionStandard() { return 220; }

// --- Object names -----------------------------------------------------------
QString LegacyTheme::cardObjectName() { return QStringLiteral("LegacyCard"); }
QString LegacyTheme::pageObjectName() { return QStringLiteral("LegacyPage"); }
QString LegacyTheme::exitButtonObjectName() { return QStringLiteral("LegacyExitButton"); }
QString LegacyTheme::statusDotObjectName() { return QStringLiteral("LegacyStatusDot"); }

QString LegacyTheme::styleSheet() {
    // A single centralized style sheet. Selectors are scoped by object name and
    // dynamic properties so only DarkSpark widgets are affected and styling is
    // not duplicated inline. No external fonts or icons are referenced.
    //
    // Card visual states are driven by the "legacyState" dynamic property set
    // by DashboardCard. Accent is driven by "legacyAccent". Because Qt Style
    // Sheets only re-evaluate property selectors when the property changes and
    // style is re-polished, DashboardCard re-polishes itself on state changes.
    const QString base = backgroundBase().name();
    const QString raised = backgroundRaised().name();
    const QString overlay = backgroundOverlay().name();
    const QString subtle = borderSubtle().name();
    const QString active = borderActive().name();
    const QString cyan = accentCyan().name();
    const QString purple = accentPurple().name();
    const QString textPri = textPrimary().name();
    const QString textSec = textSecondary().name();
    const QString textDis = textDisabled().name();
    const QString warn = statusWarning().name();
    const QString err = statusError().name();

    QString sheet;

    // Base widget + page container.
    sheet += QStringLiteral(
                 "QWidget { background-color: %1; color: %2; font-size: %3px; }"
                 "QWidget#%4 { background-color: transparent; }")
                 .arg(base, textPri)
                 .arg(fontSizeBody())
                 .arg(pageObjectName());

    // Card: raised surface, thin illuminated border, rounded. Default (Normal).
    sheet += QStringLiteral(
                 "QFrame#%1 { background-color: %2; border: %3px solid %4;"
                 " border-radius: %5px; }")
                 .arg(cardObjectName(), raised)
                 .arg(borderHairline())
                 .arg(subtle)
                 .arg(radiusLg());

    // Focus + pressed feedback (keyboard focus visible; touch press feedback).
    sheet += QStringLiteral(
                 "QFrame#%1[legacyState=\"focused\"] { border: %2px solid %3; }"
                 "QFrame#%1[legacyState=\"pressed\"] { border: %2px solid %4;"
                 " background-color: %5; }")
                 .arg(cardObjectName())
                 .arg(borderThin())
                 .arg(active, cyan, overlay);

    // Non-normal content states. Each pairs a border/text tint with a distinct
    // status label + dot (color is never the sole signal; see DashboardCard).
    sheet += QStringLiteral(
                 "QFrame#%1[legacyState=\"loading\"] { border: %2px solid %3; }"
                 "QFrame#%1[legacyState=\"empty\"] { border: %4px dashed %3; }"
                 "QFrame#%1[legacyState=\"unavailable\"] { border: %4px dashed %5; }"
                 "QFrame#%1[legacyState=\"warning\"] { border: %2px solid %6; }"
                 "QFrame#%1[legacyState=\"error\"] { border: %2px solid %7; }"
                 "QFrame#%1[legacyState=\"disabled\"] { border: %4px solid %5;"
                 " background-color: %8; }")
                 .arg(cardObjectName())
                 .arg(borderThin())
                 .arg(subtle)
                 .arg(borderHairline())
                 .arg(textDis, warn, err, base);

    // Accent roles: a subtle top-of-mind accent on the border when requested.
    sheet += QStringLiteral(
                 "QFrame#%1[legacyAccent=\"cyan\"] { border-color: %2; }"
                 "QFrame#%1[legacyAccent=\"purple\"] { border-color: %3; }")
                 .arg(cardObjectName(), cyan, purple);

    // Text roles inside cards.
    sheet += QStringLiteral(
                 "QLabel[legacyRole=\"cardTitle\"] { color: %1; font-size: %2px;"
                 " font-weight: 600; }"
                 "QLabel[legacyRole=\"cardSubtitle\"] { color: %3; font-size: %4px; }"
                 "QLabel[legacyRole=\"cardStatus\"] { color: %3; font-size: %5px; }"
                 "QLabel[legacyRole=\"pageTitle\"] { color: %6; font-size: %7px;"
                 " font-weight: 600; }"
                 "QLabel[legacyRole=\"cardStatus\"][legacyState=\"warning\"]"
                 " { color: %8; }"
                 "QLabel[legacyRole=\"cardStatus\"][legacyState=\"error\"]"
                 " { color: %9; }"
                 "QLabel[legacyRole=\"cardTitle\"][legacyState=\"disabled\"]"
                 " { color: %10; }")
                 .arg(cyan)
                 .arg(fontSizeSubtitle())
                 .arg(textSec)
                 .arg(fontSizeBody())
                 .arg(fontSizeCaption())
                 .arg(textPri)
                 .arg(fontSizeTitle())
                 .arg(warn, err, textDis);

    // Status indicator dot: a small shaped indicator. Shape/opacity plus the
    // status text carry meaning so color is not the only signal.
    sheet += QStringLiteral(
                 "QLabel#%1 { font-size: %2px; }")
                 .arg(statusDotObjectName())
                 .arg(fontSizeBody());

    // Buttons: touch-sized, thin border, cyan on focus/press. (Exit control.)
    sheet += QStringLiteral(
                 "QPushButton { background-color: %1; color: %2; border: %3px solid %4;"
                 " border-radius: %5px; padding: 10px 18px; min-height: %6px; }"
                 "QPushButton:hover, QPushButton:focus { border-color: %7; color: %8; }"
                 "QPushButton:pressed { border-color: %9; }"
                 "QPushButton:disabled { color: %10; border-color: %4; }"
                 "QPushButton#%11:hover, QPushButton#%11:focus { border-color: %12; }")
                 .arg(overlay, textPri)
                 .arg(borderHairline())
                 .arg(subtle)
                 .arg(radiusMd())
                 .arg(touchTargetMin())
                 .arg(active, cyan, purple, textDis)
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
