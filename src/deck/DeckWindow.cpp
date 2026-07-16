// SPDX-License-Identifier: GPL-3.0-or-later
#include "deck/DeckWindow.hpp"

#include "deck/cards/DashboardCard.hpp"
#include "deck/navigation/PageManager.hpp"
#include "deck/pages/DeckPage.hpp"
#include "themes/LegacyTheme.hpp"

#include <QGuiApplication>
#include <QKeyEvent>
#include <QPushButton>
#include <QScreen>
#include <QVBoxLayout>
#include <QWindow>

#include <initializer_list>
#include <utility>

namespace darkspark::deck {

using cards::DashboardCard;
using navigation::PageManager;
using pages::DeckPage;
using themes::LegacyTheme;

namespace {
// The five placeholder pages required by the Deck-0 spec, with a couple of
// placeholder cards each. No integration logic — titles and status text only.
struct PagePlan {
    const char* title;
    std::initializer_list<std::pair<const char*, const char*>> cards;
};

const std::initializer_list<PagePlan> kPagePlans = {
    {"Command",
     {{"Quick Actions", "Placeholder"}, {"Shortcuts", "Placeholder"}}},
    {"System",
     {{"Overview", "No data source"}, {"Load", "No data source"}}},
    {"Media",
     {{"Now Playing", "Placeholder"}, {"Transport", "Placeholder"}}},
    {"Communications",
     {{"Messages", "Placeholder"}, {"Presence", "Placeholder"}}},
    {"Home",
     {{"Rooms", "Placeholder"}, {"Scenes", "Placeholder"}}},
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
    exitButton->setMinimumSize(LegacyTheme::touchTargetMin() * 2,
                               LegacyTheme::touchTargetMin());
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
        for (const auto& card : plan.cards) {
            page->addCard(new DashboardCard(QString::fromUtf8(card.first),
                                            QString::fromUtf8(card.second)));
        }
        pageManager_->addPage(page);
    }
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
