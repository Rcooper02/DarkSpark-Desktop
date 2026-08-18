// SPDX-License-Identifier: GPL-3.0-or-later
#include "deck/pages/CompanionControlsPage.hpp"

#include "deck/controls/CompanionControl.hpp"
#include "deck/controls/CompanionControlTile.hpp"
#include "services/CompanionClient.hpp"
#include "themes/LegacyTheme.hpp"

#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QTimer>
#include <QVBoxLayout>

#include <array>

namespace darkspark::deck::pages {

namespace {
constexpr int HEALTH_INTERVAL_MS = 5000;

using controls::CompanionControl;

const std::array<CompanionControl, 1> DEFAULT_CONTROLS{{
    {QStringLiteral("COMPANION TEST"),
     QStringLiteral("PAGE 1  •  ROW 0  •  COL 3"), 1, 0, 3},
}};
}  // namespace

CompanionControlsPage::CompanionControlsPage(services::CompanionClient* client,
                                             QWidget* parent)
    : QWidget(parent), client_(client) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(themes::LegacyTheme::space2xl(),
                             themes::LegacyTheme::spaceXl(),
                             themes::LegacyTheme::space2xl(),
                             themes::LegacyTheme::spaceXl());
    root->setSpacing(themes::LegacyTheme::spaceLg());

    auto* title = new QLabel(QStringLiteral("CONTROLS"), this);
    title->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    title->setStyleSheet(QStringLiteral(
        "color: %1; font-size: %2px; font-weight: 700; letter-spacing: 1px;")
                             .arg(themes::LegacyTheme::textPrimary().name())
                             .arg(themes::LegacyTheme::fontPageTitle()));
    root->addWidget(title);

    statusLabel_ = new QLabel(QStringLiteral("COMPANION — CHECKING"), this);
    statusLabel_->setStyleSheet(QStringLiteral("color: %1; font-size: %2px;")
                                    .arg(themes::LegacyTheme::textSecondary().name())
                                    .arg(themes::LegacyTheme::fontStatus()));
    root->addWidget(statusLabel_);

    auto* gridHost = new QWidget(this);
    auto* grid = new QGridLayout(gridHost);
    grid->setContentsMargins(0, themes::LegacyTheme::spaceMd(), 0, 0);
    grid->setHorizontalSpacing(themes::LegacyTheme::spaceLg());
    grid->setVerticalSpacing(themes::LegacyTheme::spaceLg());

    int slot = 0;
    for (const CompanionControl& control : DEFAULT_CONTROLS) {
        const int row = slot / 3;
        const int column = slot % 3;
        grid->addWidget(new controls::CompanionControlTile(control, client_, gridHost),
                        row, column);
        ++slot;
    }

    // Keep the future grid shape visible without inventing actions. These are
    // not buttons and cannot be activated; they simply reserve capacity for the
    // forthcoming control editor.
    while (slot < 6) {
        auto* empty = new QFrame(gridHost);
        empty->setMinimumSize(220, 112);
        empty->setStyleSheet(QStringLiteral(
            "QFrame { background: %1; border: 1px dashed %2; border-radius: %3px; }")
                                 .arg(themes::LegacyTheme::backgroundRaised().name(),
                                      themes::LegacyTheme::borderSubtle().name())
                                 .arg(themes::LegacyTheme::radiusMd()));
        auto* emptyLayout = new QVBoxLayout(empty);
        auto* emptyLabel = new QLabel(QStringLiteral("EMPTY"), empty);
        emptyLabel->setAlignment(Qt::AlignCenter);
        emptyLabel->setStyleSheet(QStringLiteral("color: %1; font-size: %2px;")
                                      .arg(themes::LegacyTheme::textDisabled().name())
                                      .arg(themes::LegacyTheme::fontStatus()));
        emptyLayout->addWidget(emptyLabel);
        grid->addWidget(empty, slot / 3, slot % 3);
        ++slot;
    }

    for (int column = 0; column < 3; ++column) {
        grid->setColumnStretch(column, 1);
    }
    for (int row = 0; row < 2; ++row) {
        grid->setRowStretch(row, 1);
    }
    root->addWidget(gridHost, 1);

    if (client_ == nullptr) {
        statusLabel_->setText(QStringLiteral("COMPANION — CLIENT UNAVAILABLE"));
        return;
    }

    connect(client_, &services::CompanionClient::availabilityChanged, this,
            [this](bool available) {
                statusLabel_->setText(available
                    ? QStringLiteral("COMPANION — ONLINE")
                    : QStringLiteral("COMPANION — OFFLINE"));
                statusLabel_->setStyleSheet(QStringLiteral("color: %1; font-size: %2px;")
                    .arg(available ? themes::LegacyTheme::statusGood().name()
                                   : themes::LegacyTheme::statusError().name())
                    .arg(themes::LegacyTheme::fontStatus()));
            });

    auto* healthTimer = new QTimer(this);
    healthTimer->setInterval(HEALTH_INTERVAL_MS);
    connect(healthTimer, &QTimer::timeout, client_,
            &services::CompanionClient::checkHealth);
    healthTimer->start();
    client_->checkHealth();
}

}  // namespace darkspark::deck::pages
