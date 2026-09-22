// SPDX-License-Identifier: GPL-3.0-or-later
#include "deck/cards/ControlDeckCard.hpp"

#include "deck/cards/GlassActionButton.hpp"
#include "themes/LegacyTheme.hpp"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

#include <array>

namespace darkspark::deck::cards {

namespace {
struct ButtonPlan {
    models::ControlAction action;
    const char* label;
};

constexpr std::array<ButtonPlan, 6> kButtons{{
    {models::ControlAction::LaunchFirefox, "FIREFOX"},
    {models::ControlAction::LaunchSteam, "STEAM"},
    {models::ControlAction::LaunchTerminal, "TERMINAL"},
    {models::ControlAction::LaunchFiles, "FILES"},
    {models::ControlAction::LaunchSystemMonitor, "SYSTEM MONITOR"},
    {models::ControlAction::LockSession, "LOCK"},
}};

constexpr std::array<ButtonPlan, 6> kMediaButtons{{
    {models::ControlAction::PreviousTrack, "PREVIOUS"},
    {models::ControlAction::PlayPause, "PLAY / PAUSE"},
    {models::ControlAction::NextTrack, "NEXT"},
    {models::ControlAction::VolumeDown, "VOLUME −"},
    {models::ControlAction::ToggleMute, "MUTE"},
    {models::ControlAction::VolumeUp, "VOLUME +"},
}};

constexpr int kDeckColumns = 5;
constexpr int kDeckRows = 3;
}  // namespace

ControlDeckCard::ControlDeckCard(QWidget* parent)
    : DashboardCard(QStringLiteral("CONTROL DECK"), parent) {
    setSubtitle(QStringLiteral("Allow-listed Fedora launch controls"));
    setAccent(Accent::Cyan);
    setSizeRole(Size::Full);
    setStatusText(QStringLiteral("Ready"));

    auto* content = new QWidget(this);
    auto* split = new QHBoxLayout(content);
    split->setContentsMargins(0, 0, 0, 0);
    split->setSpacing(themes::LegacyTheme::spaceXl());

    auto* keyField = new QWidget(content);
    auto* grid = new QGridLayout(keyField);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(themes::LegacyTheme::spaceLg());
    grid->setVerticalSpacing(themes::LegacyTheme::spaceLg());
    for (int index = 0; index < kDeckColumns * kDeckRows; ++index) {
        GlassActionButton* button = nullptr;
        if (index < static_cast<int>(kButtons.size())) {
            const ButtonPlan plan = kButtons.at(static_cast<std::size_t>(index));
            button = new GlassActionButton(QString::fromUtf8(plan.label), keyField);
            connect(button, &QPushButton::clicked, this,
                    [this, action = plan.action]() { emit actionRequested(action); });
        } else {
            button = new GlassActionButton(QStringLiteral("AVAILABLE"), keyField);
            button->setEnabled(false);
        }
        grid->addWidget(button, index / kDeckColumns, index % kDeckColumns);
    }
    split->addWidget(keyField, 1);

    auto* mediaConsole = new QFrame(content);
    mediaConsole->setObjectName(themes::LegacyTheme::mediaConsoleObjectName());
    mediaConsole->setMinimumWidth(510);
    mediaConsole->setMaximumWidth(620);
    auto* mediaLayout = new QVBoxLayout(mediaConsole);
    mediaLayout->setContentsMargins(themes::LegacyTheme::spaceLg(),
                                    themes::LegacyTheme::spaceLg(),
                                    themes::LegacyTheme::spaceLg(),
                                    themes::LegacyTheme::spaceLg());
    mediaLayout->setSpacing(themes::LegacyTheme::spaceMd());

    auto* mediaTitle = new QLabel(QStringLiteral("MEDIA CONTROL CENTER"), mediaConsole);
    mediaTitle->setProperty("legacyRole", "cardTitle");
    mediaLayout->addWidget(mediaTitle);

    mediaDisplay_ = new QLabel(
        QStringLiteral("NO ACTIVE PLAYER\nREADY FOR MEDIA"), mediaConsole);
    mediaDisplay_->setObjectName(themes::LegacyTheme::mediaDisplayObjectName());
    mediaDisplay_->setAlignment(Qt::AlignCenter);
    mediaDisplay_->setMinimumHeight(96);
    mediaLayout->addWidget(mediaDisplay_);

    auto* mediaGrid = new QGridLayout();
    mediaGrid->setHorizontalSpacing(themes::LegacyTheme::spaceSm());
    mediaGrid->setVerticalSpacing(themes::LegacyTheme::spaceSm());
    for (std::size_t index = 0; index < kMediaButtons.size(); ++index) {
        const ButtonPlan plan = kMediaButtons.at(index);
        auto* button = new GlassActionButton(QString::fromUtf8(plan.label), mediaConsole);
        // The media console is intentionally denser than the primary key field;
        // keep its glass controls touch-safe without forcing vertical overflow
        // on the 720-pixel XENEON canvas.
        button->setMinimumSize(104, 72);
        connect(button, &QPushButton::clicked, this,
                [this, action = plan.action]() { emit actionRequested(action); });
        mediaGrid->addWidget(button, static_cast<int>(index / 3U),
                             static_cast<int>(index % 3U));
    }
    mediaLayout->addLayout(mediaGrid, 1);
    split->addWidget(mediaConsole, 0);
    setContentWidget(content);
}

void ControlDeckCard::reportResult(models::ControlAction action, bool success,
                                   const QString& message) {
    setState(success ? State::Normal : State::Warning);
    setStatusText(message);
    if (mediaDisplay_ != nullptr && models::isAudioAction(action)) {
        mediaDisplay_->setText(success ? message.toUpper()
                                       : QStringLiteral("CONTROL ERROR\n%1")
                                             .arg(message.toUpper()));
    }
}

}  // namespace darkspark::deck::cards
