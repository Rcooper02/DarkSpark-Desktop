// SPDX-License-Identifier: GPL-3.0-or-later
#include "deck/pages/CompanionControlsPage.hpp"

#include "services/CompanionClient.hpp"
#include "themes/LegacyTheme.hpp"

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace darkspark::deck::pages {

namespace {
constexpr int COMPANION_TEST_PAGE = 1;
constexpr int COMPANION_TEST_ROW = 0;
constexpr int COMPANION_TEST_COLUMN = 3;
}  // namespace

CompanionControlsPage::CompanionControlsPage(services::CompanionClient* client,
                                             QWidget* parent)
    : QWidget(parent), client_(client) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(themes::LegacyTheme::space3xl(),
                               themes::LegacyTheme::space3xl(),
                               themes::LegacyTheme::space3xl(),
                               themes::LegacyTheme::space3xl());
    layout->setSpacing(themes::LegacyTheme::spaceXl());
    layout->addStretch(1);

    auto* title = new QLabel(QStringLiteral("COMPANION CONTROL SPIKE"), this);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet(QStringLiteral("color: %1; font-size: %2px; font-weight: 700;")
                             .arg(themes::LegacyTheme::textPrimary().name())
                             .arg(themes::LegacyTheme::fontPageTitle()));
    layout->addWidget(title);

    auto* subtitle = new QLabel(
        QStringLiteral("Native DarkSpark control → Companion 1 / 0 / 3"), this);
    subtitle->setAlignment(Qt::AlignCenter);
    subtitle->setStyleSheet(QStringLiteral("color: %1; font-size: %2px;")
                                .arg(themes::LegacyTheme::textSecondary().name())
                                .arg(themes::LegacyTheme::fontSupporting()));
    layout->addWidget(subtitle);

    testButton_ = new QPushButton(QStringLiteral("COMPANION TEST"), this);
    testButton_->setMinimumSize(260, themes::LegacyTheme::touchTargetPreferred());
    testButton_->setCursor(Qt::PointingHandCursor);
    testButton_->setStyleSheet(QStringLiteral(
        "QPushButton { background: %1; color: %2; border: 2px solid %3;"
        " border-radius: %4px; padding: 12px 28px; font-weight: 700;"
        " letter-spacing: 1px; }"
        "QPushButton:hover { background: %5; border-color: %6; }"
        "QPushButton:pressed { background: %5; color: %7; border-color: %6; }")
                                   .arg(themes::LegacyTheme::backgroundRaised().name(),
                                        themes::LegacyTheme::accentCyan().name(),
                                        themes::LegacyTheme::borderStrong().name())
                                   .arg(themes::LegacyTheme::radiusSm())
                                   .arg(themes::LegacyTheme::backgroundOverlay().name(),
                                        themes::LegacyTheme::accentCyan().name(),
                                        themes::LegacyTheme::textPrimary().name()));
    layout->addWidget(testButton_, 0, Qt::AlignCenter);

    statusLabel_ = new QLabel(QStringLiteral("READY — Companion localhost:8000"), this);
    statusLabel_->setAlignment(Qt::AlignCenter);
    statusLabel_->setStyleSheet(QStringLiteral("color: %1; font-size: %2px;")
                                    .arg(themes::LegacyTheme::textSecondary().name())
                                    .arg(themes::LegacyTheme::fontStatus()));
    layout->addWidget(statusLabel_);
    layout->addStretch(1);

    if (client_ == nullptr) {
        testButton_->setEnabled(false);
        statusLabel_->setText(QStringLiteral("COMPANION CLIENT UNAVAILABLE"));
        return;
    }

    connect(testButton_, &QPushButton::pressed, this, [this] {
        statusLabel_->setText(QStringLiteral("SENDING DOWN…"));
        client_->down(COMPANION_TEST_PAGE, COMPANION_TEST_ROW,
                      COMPANION_TEST_COLUMN);
    });
    connect(testButton_, &QPushButton::released, this, [this] {
        statusLabel_->setText(QStringLiteral("SENDING UP…"));
        client_->up(COMPANION_TEST_PAGE, COMPANION_TEST_ROW,
                    COMPANION_TEST_COLUMN);
    });

    connect(client_, &services::CompanionClient::requestSucceeded, this,
            [this](const QString& action, int page, int row, int column) {
                if (page == COMPANION_TEST_PAGE && row == COMPANION_TEST_ROW
                    && column == COMPANION_TEST_COLUMN) {
                    statusLabel_->setText(
                        QStringLiteral("COMPANION OK — %1").arg(action.toUpper()));
                }
            });
    connect(client_, &services::CompanionClient::requestFailed, this,
            [this](const QString& action, int page, int row, int column,
                   const QString& error) {
                if (page == COMPANION_TEST_PAGE && row == COMPANION_TEST_ROW
                    && column == COMPANION_TEST_COLUMN) {
                    statusLabel_->setText(
                        QStringLiteral("COMPANION %1 FAILED — %2")
                            .arg(action.toUpper(), error));
                }
            });
}

}  // namespace darkspark::deck::pages
