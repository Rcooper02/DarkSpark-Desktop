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
int LegacyTheme::spaceSm() { return 8; }
int LegacyTheme::spaceMd() { return 12; }
int LegacyTheme::spaceLg() { return 16; }
int LegacyTheme::spaceXl() { return 24; }
int LegacyTheme::borderHairline() { return 1; }
int LegacyTheme::radiusMd() { return 8; }
int LegacyTheme::radiusLg() { return 12; }
int LegacyTheme::touchTargetMin() { return 44; }

// --- Object names -----------------------------------------------------------
QString LegacyTheme::cardObjectName() { return QStringLiteral("LegacyCard"); }
QString LegacyTheme::pageObjectName() { return QStringLiteral("LegacyPage"); }
QString LegacyTheme::exitButtonObjectName() { return QStringLiteral("LegacyExitButton"); }

QString LegacyTheme::styleSheet() {
    // A single centralized style sheet. Selectors are scoped by object name so
    // that only DarkSpark widgets are affected and styling is not duplicated
    // inline across the UI. No external fonts or icons are referenced.
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

    return QStringLiteral(
               "QWidget {"
               "  background-color: %1;"
               "  color: %8;"
               "  font-size: 15px;"
               "}"
               // Page container: transparent so a DeckWindow background shows.
               "QWidget#%12 {"
               "  background-color: transparent;"
               "}"
               // Card: raised surface, thin illuminated border, rounded.
               "QFrame#%13 {"
               "  background-color: %2;"
               "  border: %16px solid %4;"
               "  border-radius: %15px;"
               "}"
               // Titles and secondary text inside cards.
               "QLabel[legacyRole=\"cardTitle\"] {"
               "  color: %6;"
               "  font-size: 18px;"
               "  font-weight: 600;"
               "}"
               "QLabel[legacyRole=\"cardStatus\"] {"
               "  color: %9;"
               "  font-size: 13px;"
               "}"
               "QLabel[legacyRole=\"pageTitle\"] {"
               "  color: %8;"
               "  font-size: 22px;"
               "  font-weight: 600;"
               "}"
               // Buttons: touch-sized, thin border, cyan on focus/press.
               "QPushButton {"
               "  background-color: %3;"
               "  color: %8;"
               "  border: %16px solid %4;"
               "  border-radius: %14px;"
               "  padding: 10px 18px;"
               "  min-height: %17px;"
               "}"
               "QPushButton:hover, QPushButton:focus {"
               "  border-color: %5;"
               "  color: %6;"
               "}"
               "QPushButton:pressed {"
               "  border-color: %7;"
               "}"
               "QPushButton:disabled {"
               "  color: %10;"
               "  border-color: %4;"
               "}"
               // Exit control: same touch sizing, error accent on hover/press.
               "QPushButton#%11:hover, QPushButton#%11:focus {"
               "  border-color: %7;"
               "}")
        .arg(base,        // %1
             raised,      // %2
             overlay,     // %3
             subtle,      // %4
             active,      // %5
             cyan,        // %6
             purple,      // %7
             textPri)     // %8
        .arg(textSec)     // %9
        .arg(textDis)     // %10
        .arg(exitButtonObjectName())  // %11
        .arg(pageObjectName())        // %12
        .arg(cardObjectName())        // %13
        .arg(radiusMd())              // %14
        .arg(radiusLg())              // %15
        .arg(borderHairline())        // %16
        .arg(touchTargetMin());       // %17
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
