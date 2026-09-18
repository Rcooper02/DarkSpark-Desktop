// SPDX-License-Identifier: GPL-3.0-or-later
#include "deck/companion/CompanionCard.hpp"

#include "deck/companion/CompanionFaceWidget.hpp"
#include "themes/LegacyTheme.hpp"

#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

#include <array>

namespace darkspark::deck::companion {

using models::CompanionState;
using themes::LegacyTheme;

namespace {
struct StateControl {
    CompanionState state;
    const char* label;
};

constexpr std::array<StateControl, 6> kControls{{
    {CompanionState::Dormant, "Sleep"},
    {CompanionState::Idle, "Idle"},
    {CompanionState::Listening, "Listen"},
    {CompanionState::Thinking, "Think"},
    {CompanionState::Speaking, "Speak"},
    {CompanionState::Alert, "Alert"},
}};
}  // namespace

CompanionCard::CompanionCard(QWidget* parent)
    : DashboardCard(QStringLiteral("DARKSPARK // COMPANION"), parent),
      face_(new CompanionFaceWidget(this)),
      stateLabel_(new QLabel(this)) {
    setSubtitle(QStringLiteral("OPTICAL INTELLIGENCE CORE"));
    setAccent(Accent::None);
    setSizeRole(Size::Large);
    setStatusText(QStringLiteral("Tracking simulation active — PIXY awaiting link"));

    auto* content = new QWidget(this);
    auto* root = new QVBoxLayout(content);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(LegacyTheme::spaceMd());
    root->addWidget(face_, 1);

    stateLabel_->setProperty("legacyRole", "companionState");
    stateLabel_->setAlignment(Qt::AlignCenter);
    root->addWidget(stateLabel_);

    auto* controls = new QGridLayout();
    controls->setHorizontalSpacing(LegacyTheme::spaceSm());
    controls->setVerticalSpacing(LegacyTheme::spaceSm());
    for (std::size_t index = 0; index < kControls.size(); ++index) {
        const StateControl control = kControls.at(index);
        auto* button = new QPushButton(QString::fromUtf8(control.label), content);
        button->setMinimumHeight(LegacyTheme::touchTargetMin());
        connect(button, &QPushButton::clicked, this,
                [this, state = control.state]() { emit stateRequested(state); });
        const int column = static_cast<int>(index % 3U);
        const int row = static_cast<int>(index / 3U);
        controls->addWidget(button, row, column);
    }
    root->addLayout(controls);
    setContentWidget(content);
    setCompanionState(CompanionState::Idle);
}

void CompanionCard::setCompanionState(CompanionState state) {
    face_->setCompanionState(state);
    const std::string_view name = models::companionStateName(state);
    stateLabel_->setText(QStringLiteral("STATE // %1")
                             .arg(QString::fromUtf8(name.data(),
                                                   static_cast<qsizetype>(name.size()))));
    stateLabel_->setProperty("companionState", static_cast<int>(state));
    stateLabel_->style()->unpolish(stateLabel_);
    stateLabel_->style()->polish(stateLabel_);
}

CompanionState CompanionCard::companionState() const {
    return face_->companionState();
}

void CompanionCard::setGazeTarget(models::GazeTarget target) {
    face_->setGazeTarget(target);
    setStatusText(QStringLiteral("PIXY subject lock active"));
}

void CompanionCard::clearGazeTarget() {
    face_->clearGazeTarget();
    setStatusText(QStringLiteral("Tracking simulation active — PIXY awaiting link"));
}

}  // namespace darkspark::deck::companion
