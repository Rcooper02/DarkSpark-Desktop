// SPDX-License-Identifier: GPL-3.0-or-later
#include "deck/controls/CompanionControlTile.hpp"

#include "services/CompanionClient.hpp"
#include "themes/LegacyTheme.hpp"

namespace darkspark::deck::controls {

CompanionControlTile::CompanionControlTile(const CompanionControl& control,
                                           services::CompanionClient* client,
                                           QWidget* parent)
    : QPushButton(parent), control_(control), client_(client) {
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::NoFocus);
    setMinimumSize(220, 112);
    setCheckable(false);

    if (client_ == nullptr || !control_.isValid()) {
        companionAvailable_ = false;
        setEnabled(false);
        refreshPresentation();
        return;
    }

    companionAvailable_ = !client_->availabilityKnown() || client_->available();
    refreshPresentation();

    connect(this, &QPushButton::pressed, this, [this] {
        client_->down(control_.page, control_.row, control_.column);
    });
    connect(this, &QPushButton::released, this, [this] {
        client_->up(control_.page, control_.row, control_.column);
    });
    connect(client_, &services::CompanionClient::availabilityChanged, this,
            [this](bool available) {
                companionAvailable_ = available;
                setEnabled(available);
                refreshPresentation();
            });
}

void CompanionControlTile::refreshPresentation() {
    QString text = control_.label;
    if (!control_.subtitle.trimmed().isEmpty()) {
        text += QStringLiteral("\n%1").arg(control_.subtitle);
    }
    if (!companionAvailable_) {
        text += QStringLiteral("\nCOMPANION OFFLINE");
    }
    setText(text);

    const QString style = QStringLiteral(
        "QPushButton { background: %1; color: %2; border: 1px solid %3;"
        " border-radius: %4px; padding: 14px 18px; font-weight: 700;"
        " letter-spacing: 1px; }"
        "QPushButton:hover { background: %5; color: %6; border: 2px solid %7; }"
        "QPushButton:pressed { background: %5; color: %6; border: 2px solid %7; }"
        "QPushButton:disabled { background: %8; color: %9; border: 1px solid %10; }")
                              .arg(themes::LegacyTheme::backgroundRaised().name(),
                                   themes::LegacyTheme::accentCyan().name(),
                                   themes::LegacyTheme::borderStrong().name())
                              .arg(themes::LegacyTheme::radiusMd())
                              .arg(themes::LegacyTheme::backgroundOverlay().name(),
                                   themes::LegacyTheme::textPrimary().name(),
                                   themes::LegacyTheme::accentCyan().name(),
                                   themes::LegacyTheme::backgroundDisabled().name(),
                                   themes::LegacyTheme::textDisabled().name(),
                                   themes::LegacyTheme::borderSubtle().name());
    setStyleSheet(style);
}

}  // namespace darkspark::deck::controls
