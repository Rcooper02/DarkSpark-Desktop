// SPDX-License-Identifier: GPL-3.0-or-later
#include "deck/pages/DeckPage.hpp"

#include "deck/cards/DashboardCard.hpp"
#include "themes/LegacyTheme.hpp"

#include <QGridLayout>
#include <QLabel>
#include <QResizeEvent>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace darkspark::deck::pages {

using cards::DashboardCard;
using themes::LegacyTheme;

namespace {
// Target width budget per single-column card. Column count is derived from the
// available content width divided by this, then clamped. Chosen so 2560px wide
// yields a wide multi-column deck while a small window collapses gracefully.
constexpr int kTargetCardWidth = 300;
constexpr int kMinColumns = 1;
constexpr int kMaxColumns = 6;
}  // namespace

DeckPage::DeckPage(QString title, QString subtitle, QWidget* parent)
    : QWidget(parent), title_(std::move(title)), subtitle_(std::move(subtitle)),
      grid_(new QGridLayout) {
    setObjectName(LegacyTheme::pageObjectName());

    auto* root = new QVBoxLayout(this);
    // Compact top/bottom margins so the header does not consume excessive
    // vertical space on the 2560x720 target (docs/VISUAL_LANGUAGE.md).
    root->setContentsMargins(LegacyTheme::space2xl(), LegacyTheme::spaceLg(),
                             LegacyTheme::space2xl(), LegacyTheme::spaceLg());
    root->setSpacing(LegacyTheme::spaceLg());

    // --- Page header: title over a quieter subtitle, tightly grouped --------
    auto* headerBlock = new QWidget(this);
    headerBlock->setObjectName(LegacyTheme::pageHeaderObjectName());
    auto* headerCol = new QVBoxLayout(headerBlock);
    headerCol->setContentsMargins(0, 0, 0, 0);
    headerCol->setSpacing(LegacyTheme::spaceXs());

    auto* titleLabel = new QLabel(title_, headerBlock);
    titleLabel->setProperty("legacyRole", "pageTitle");
    headerCol->addWidget(titleLabel);

    if (!subtitle_.isEmpty()) {
        auto* subtitleLabel = new QLabel(subtitle_, headerBlock);
        subtitleLabel->setProperty("legacyRole", "pageSubtitle");
        headerCol->addWidget(subtitleLabel);
    }
    root->addWidget(headerBlock);

    grid_->setHorizontalSpacing(LegacyTheme::spaceLg());
    grid_->setVerticalSpacing(LegacyTheme::spaceLg());
    root->addLayout(grid_);
    root->addStretch(1);
}

void DeckPage::addCard(DashboardCard* card) {
    if (card == nullptr) {
        return;
    }
    card->setParent(this);
    cards_.append(card);
    // Force a relayout with the current width on next event; do it now so the
    // card is placed immediately even before the first resize.
    currentColumns_ = 0;  // invalidate so relayout re-runs
    const int contentWidth = width() > 0 ? width() : (kTargetCardWidth * 4);
    relayout(columnsForWidth(contentWidth));
}

QString DeckPage::title() const { return title_; }

QString DeckPage::subtitle() const { return subtitle_; }

int DeckPage::cardCount() const { return static_cast<int>(cards_.size()); }

int DeckPage::columnsForWidth(int contentWidth) const {
    const int margins = LegacyTheme::spaceXl() * 2;
    const int usable = std::max(contentWidth - margins, kTargetCardWidth);
    int columns = usable / kTargetCardWidth;
    columns = std::clamp(columns, kMinColumns, kMaxColumns);
    return columns;
}

int DeckPage::spanForSize(const DashboardCard* card, int columns) {
    if (card == nullptr) {
        return 1;
    }
    int span = 1;
    switch (card->sizeRole()) {
    case DashboardCard::Size::Small:
        span = 1;
        break;
    case DashboardCard::Size::Medium:
        span = 1;
        break;
    case DashboardCard::Size::Large:
        span = 2;
        break;
    case DashboardCard::Size::Wide:
        span = 3;
        break;
    }
    return std::clamp(span, 1, columns);
}

void DeckPage::relayout(int columns) {
    if (columns == currentColumns_) {
        return;
    }
    currentColumns_ = columns;

    // Remove all cards from the grid without deleting them, then re-add.
    for (DashboardCard* card : cards_) {
        grid_->removeWidget(card);
    }

    int row = 0;
    int col = 0;
    for (DashboardCard* card : cards_) {
        const int span = spanForSize(card, columns);
        // Wrap to the next row if this card would overflow the row.
        if (col + span > columns) {
            row += 1;
            col = 0;
        }
        grid_->addWidget(card, row, col, 1, span);
        col += span;
        if (col >= columns) {
            row += 1;
            col = 0;
        }
    }

    // Keep columns evenly stretched so cards fill the width consistently.
    for (int c = 0; c < kMaxColumns; ++c) {
        grid_->setColumnStretch(c, c < columns ? 1 : 0);
    }
}

void DeckPage::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    relayout(columnsForWidth(event->size().width()));
}

}  // namespace darkspark::deck::pages
