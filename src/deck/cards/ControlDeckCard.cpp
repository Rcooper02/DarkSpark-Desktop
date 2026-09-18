// SPDX-License-Identifier: GPL-3.0-or-later
#include "deck/cards/ControlDeckCard.hpp"

#include "themes/LegacyTheme.hpp"

#include <QGridLayout>
#include <QPushButton>

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
}  // namespace

ControlDeckCard::ControlDeckCard(QWidget* parent)
    : DashboardCard(QStringLiteral("CONTROL DECK"), parent) {
    setSubtitle(QStringLiteral("Allow-listed Fedora launch controls"));
    setAccent(Accent::Cyan);
    setSizeRole(Size::Wide);
    setStatusText(QStringLiteral("Ready"));

    auto* content = new QWidget(this);
    auto* grid = new QGridLayout(content);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(themes::LegacyTheme::spaceMd());
    grid->setVerticalSpacing(themes::LegacyTheme::spaceMd());
    for (std::size_t index = 0; index < kButtons.size(); ++index) {
        const ButtonPlan plan = kButtons.at(index);
        auto* button = new QPushButton(QString::fromUtf8(plan.label), content);
        button->setMinimumHeight(themes::LegacyTheme::touchTargetPreferred() * 2);
        connect(button, &QPushButton::clicked, this,
                [this, action = plan.action]() { emit actionRequested(action); });
        grid->addWidget(button, static_cast<int>(index / 3U),
                        static_cast<int>(index % 3U));
    }
    setContentWidget(content);
}

void ControlDeckCard::reportResult(bool success, const QString& message) {
    setState(success ? State::Normal : State::Warning);
    setStatusText(message);
}

}  // namespace darkspark::deck::cards
