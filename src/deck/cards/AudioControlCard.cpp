// SPDX-License-Identifier: GPL-3.0-or-later
#include "deck/cards/AudioControlCard.hpp"

#include "themes/LegacyTheme.hpp"

#include <QGridLayout>
#include <QPushButton>
#include <QVBoxLayout>

#include <array>

namespace darkspark::deck::cards {

namespace {
struct ButtonPlan {
    models::ControlAction action;
    const char* label;
};

constexpr std::array<ButtonPlan, 6> kButtons{{
    {models::ControlAction::PreviousTrack, "PREVIOUS"},
    {models::ControlAction::PlayPause, "PLAY / PAUSE"},
    {models::ControlAction::NextTrack, "NEXT"},
    {models::ControlAction::VolumeDown, "VOLUME −"},
    {models::ControlAction::ToggleMute, "MUTE"},
    {models::ControlAction::VolumeUp, "VOLUME +"},
}};
}  // namespace

AudioControlCard::AudioControlCard(QWidget* parent)
    : DashboardCard(QStringLiteral("AUDIO CONTROL"), parent) {
    setSubtitle(QStringLiteral("PipeWire output and media transport"));
    setAccent(Accent::Purple);
    setSizeRole(Size::Wide);
    setStatusText(QStringLiteral("Ready"));

    auto* content = new QWidget(this);
    auto* grid = new QGridLayout(content);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(themes::LegacyTheme::spaceSm());
    grid->setVerticalSpacing(themes::LegacyTheme::spaceSm());
    for (std::size_t index = 0; index < kButtons.size(); ++index) {
        const ButtonPlan plan = kButtons.at(index);
        auto* button = new QPushButton(QString::fromUtf8(plan.label), content);
        button->setMinimumHeight(themes::LegacyTheme::touchTargetPreferred());
        connect(button, &QPushButton::clicked, this,
                [this, action = plan.action]() { emit actionRequested(action); });
        grid->addWidget(button, static_cast<int>(index / 3U),
                        static_cast<int>(index % 3U));
    }
    setContentWidget(content);
}

void AudioControlCard::reportResult(bool success, const QString& message) {
    setState(success ? State::Normal : State::Warning);
    setStatusText(message);
}

}  // namespace darkspark::deck::cards
