// SPDX-License-Identifier: GPL-3.0-or-later
#include "deck/DeckWindow.hpp"

#include "deck/cards/DashboardCard.hpp"
#include "deck/cards/AudioControlCard.hpp"
#include "deck/cards/ControlDeckCard.hpp"
#include "deck/companion/CompanionCard.hpp"
#include "deck/navigation/PageManager.hpp"
#include "deck/pages/DeckPage.hpp"
#include "models/MetricSample.hpp"
#include "themes/LegacyTheme.hpp"

#include <QGuiApplication>
#include <QKeyEvent>
#include <QPushButton>
#include <QScreen>
#include <QVBoxLayout>
#include <QWindow>

#include <cstring>
#include <initializer_list>

namespace darkspark::deck {

using cards::DashboardCard;
using cards::AudioControlCard;
using cards::ControlDeckCard;
using companion::CompanionCard;
using navigation::PageManager;
using pages::DeckPage;
using themes::LegacyTheme;

namespace {
// Placeholder card definition. All content is placeholder-only: no data
// source, no integration. `subtitle` may be empty. `state` and `size` exercise
// the upgraded DashboardCard presentation. A couple of non-Normal states are
// used deliberately to show the visual-state treatments; they do not represent
// real conditions.
struct CardPlan {
    const char* title;
    const char* subtitle;
    DashboardCard::Size size;
    DashboardCard::Accent accent;
    DashboardCard::State state;
};

struct PagePlan {
    const char* title;
    std::initializer_list<CardPlan> cards;
};

using S = DashboardCard::Size;
using A = DashboardCard::Accent;
using St = DashboardCard::State;

/// Title of the page that presents system telemetry. Must match the entry in
/// kPagePlans below.
constexpr const char* kSystemPageTitle = "System";
constexpr const char* kCommandPageTitle = "Command";
constexpr const char* kControlDeckPageTitle = "Control Deck";

const std::initializer_list<PagePlan> kPagePlans = {
    {"Command",
     {{"Quick Actions", "Common controls", S::Wide, A::Cyan, St::Normal},
      {"Recent Activity", "Nothing yet", S::Medium, A::None, St::Empty},
      {"System Summary", "At a glance", S::Large, A::Purple, St::Normal}}},
    {"System",
     {{"CPU", "Utilization", S::Medium, A::Cyan, St::Normal},
      {"Memory", "In use", S::Medium, A::Cyan, St::Normal},
      {"GPU", "Utilization", S::Medium, A::Purple, St::Normal},
      {"Storage", "Capacity", S::Medium, A::None, St::Normal},
      {"Network", "Throughput", S::Medium, A::None, St::Unavailable}}},
    {"Control Deck", {}},
    {"Expansion",
     {{"Future Module", "Reserved fourth screen", S::Wide, A::Purple, St::Empty},
      {"Not Configured", "Ready when its purpose is defined", S::Large,
       A::None, St::Unavailable}}},
};
}  // namespace

DeckWindow::DeckWindow(QWidget* parent)
    : QWidget(parent), pageManager_(new PageManager(this)) {
    setWindowTitle(QStringLiteral("DarkSpark Desktop — Deck Mode"));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Top bar with a visible, touch-sized Exit control. Always present so a
    // user can leave Deck Mode without a keyboard.
    auto* exitButton = new QPushButton(QStringLiteral("Exit"), this);
    exitButton->setObjectName(LegacyTheme::exitButtonObjectName());
    // Primary control: preferred 52px touch target height.
    exitButton->setMinimumSize(LegacyTheme::touchTargetMin() * 2,
                               LegacyTheme::touchTargetPreferred());
    exitButton->setCursor(Qt::PointingHandCursor);
    connect(exitButton, &QPushButton::clicked, this, &DeckWindow::exitRequested);

    auto* topBar = new QWidget(this);
    auto* topBarLayout = new QVBoxLayout(topBar);
    topBarLayout->setContentsMargins(LegacyTheme::spaceMd(), LegacyTheme::spaceMd(),
                                     LegacyTheme::spaceMd(), 0);
    topBarLayout->addWidget(exitButton, 0, Qt::AlignRight);

    root->addWidget(topBar, 0);
    root->addWidget(pageManager_, 1);

    buildPages();

    // Sized for the primary target; still resizable when windowed.
    resize(2560, 720);
}

void DeckWindow::buildPages() {
    for (const auto& plan : kPagePlans) {
        auto* page = new DeckPage(QString::fromUtf8(plan.title));
        // Capture the page that presents system telemetry so samples can be
        // routed to it without exposing pages or cards.
        if (std::strcmp(plan.title, kSystemPageTitle) == 0) {
            systemPage_ = page;
        }
        if (std::strcmp(plan.title, kCommandPageTitle) == 0) {
            companionCard_ = new CompanionCard();
            page->addCard(companionCard_);
            connect(companionCard_, &CompanionCard::stateRequested, this,
                    &DeckWindow::setCompanionState);
        }
        if (std::strcmp(plan.title, kSystemPageTitle) == 0) {
            systemAudioCard_ = new AudioControlCard();
            page->addCard(systemAudioCard_);
            connect(systemAudioCard_, &AudioControlCard::actionRequested, this,
                    &DeckWindow::controlRequested);
        }
        if (std::strcmp(plan.title, kControlDeckPageTitle) == 0) {
            controlDeckCard_ = new ControlDeckCard();
            page->addCard(controlDeckCard_);
            connect(controlDeckCard_, &ControlDeckCard::actionRequested, this,
                    &DeckWindow::controlRequested);
        }
        for (const auto& cardPlan : plan.cards) {
            auto* card = new DashboardCard(QString::fromUtf8(cardPlan.title));
            if (cardPlan.subtitle != nullptr && cardPlan.subtitle[0] != '\0') {
                card->setSubtitle(QString::fromUtf8(cardPlan.subtitle));
            }
            card->setSizeRole(cardPlan.size);
            card->setAccent(cardPlan.accent);
            card->setState(cardPlan.state);
            page->addCard(card);
        }
        pageManager_->addPage(page);
    }
}

void DeckWindow::setCompanionState(models::CompanionState state) {
    if (companionCard_ != nullptr) {
        companionCard_->setCompanionState(state);
    }
}

void DeckWindow::setCompanionGazeTarget(models::GazeTarget target) {
    if (companionCard_ != nullptr) {
        companionCard_->setGazeTarget(target);
    }
}

void DeckWindow::clearCompanionGazeTarget() {
    if (companionCard_ != nullptr) {
        companionCard_->clearGazeTarget();
    }
}

void DeckWindow::reportControlResult(models::ControlAction action, bool success,
                                     const QString& message) {
    if (systemAudioCard_ != nullptr && models::isAudioAction(action)) {
        systemAudioCard_->reportResult(success, message);
    }
    if (controlDeckCard_ != nullptr) {
        controlDeckCard_->reportResult(action, success, message);
    }
}

void DeckWindow::receiveTelemetry(const models::MetricSample& sample) {
    if (systemPage_ == nullptr) {
        // No page presents system telemetry in this window; ignore safely.
        return;
    }
    systemPage_->receiveTelemetry(sample);
}

void DeckWindow::showDeckFullscreen(QScreen* screen) {
    QScreen* target = screen != nullptr ? screen : QGuiApplication::primaryScreen();

    setWindowFlag(Qt::FramelessWindowHint, true);
    if (target != nullptr) {
        // Move onto the target screen before going fullscreen so the compositor
        // places us on the intended output.
        setGeometry(target->geometry());
        windowHandle();  // ensure a native handle exists
        if (windowHandle() != nullptr) {
            windowHandle()->setScreen(target);
        }
    }
    showFullScreen();
    pageManager_->setFocus();
}

void DeckWindow::showWindowed() {
    setWindowFlag(Qt::FramelessWindowHint, false);
    showNormal();
    pageManager_->setFocus();
}

void DeckWindow::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        // Escape must always provide an escape hatch from fullscreen.
        emit exitRequested();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

}  // namespace darkspark::deck
