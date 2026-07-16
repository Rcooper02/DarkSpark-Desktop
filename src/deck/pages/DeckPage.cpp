// SPDX-License-Identifier: GPL-3.0-or-later
#include "deck/pages/DeckPage.hpp"

#include "deck/cards/DashboardCard.hpp"
#include "themes/LegacyTheme.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

#include <utility>

namespace darkspark::deck::pages {

using themes::LegacyTheme;

DeckPage::DeckPage(QString title, QWidget* parent)
    : QWidget(parent), title_(std::move(title)), cardRow_(new QHBoxLayout) {
    setObjectName(LegacyTheme::pageObjectName());

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(LegacyTheme::spaceXl(), LegacyTheme::spaceXl(),
                             LegacyTheme::spaceXl(), LegacyTheme::spaceXl());
    root->setSpacing(LegacyTheme::spaceLg());

    auto* titleLabel = new QLabel(title_, this);
    titleLabel->setProperty("legacyRole", "pageTitle");
    root->addWidget(titleLabel);

    cardRow_->setSpacing(LegacyTheme::spaceLg());
    root->addLayout(cardRow_);
    root->addStretch(1);
}

void DeckPage::addCard(cards::DashboardCard* card) {
    if (card == nullptr) {
        return;
    }
    card->setParent(this);
    cardRow_->addWidget(card);
}

QString DeckPage::title() const { return title_; }

}  // namespace darkspark::deck::pages
