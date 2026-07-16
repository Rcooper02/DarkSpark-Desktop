// SPDX-License-Identifier: GPL-3.0-or-later
#include "deck/cards/DashboardCard.hpp"

#include "themes/LegacyTheme.hpp"

#include <QLabel>
#include <QVBoxLayout>

namespace darkspark::deck::cards {

using themes::LegacyTheme;

DashboardCard::DashboardCard(QString title, QString status, QWidget* parent)
    : QFrame(parent), titleLabel_(new QLabel(title, this)),
      statusLabel_(new QLabel(status, this)) {
    // Object name drives centralized styling from LegacyTheme's style sheet.
    setObjectName(LegacyTheme::cardObjectName());
    setFrameShape(QFrame::NoFrame);  // border comes from the style sheet

    // The "legacyRole" dynamic property lets the centralized style sheet target
    // these labels without per-widget inline styles.
    titleLabel_->setProperty("legacyRole", "cardTitle");
    statusLabel_->setProperty("legacyRole", "cardStatus");
    titleLabel_->setWordWrap(true);
    statusLabel_->setWordWrap(true);
    statusLabel_->setVisible(!status.isEmpty());

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(LegacyTheme::spaceLg(), LegacyTheme::spaceMd(),
                               LegacyTheme::spaceLg(), LegacyTheme::spaceMd());
    layout->setSpacing(LegacyTheme::spaceSm());
    layout->addWidget(titleLabel_);
    layout->addWidget(statusLabel_);
    layout->addStretch(1);

    // Ensure the card presents a reasonable touch-sized footprint.
    setMinimumHeight(LegacyTheme::touchTargetMin() * 2);
}

void DashboardCard::setTitle(const QString& title) { titleLabel_->setText(title); }

void DashboardCard::setStatus(const QString& status) {
    statusLabel_->setText(status);
    statusLabel_->setVisible(!status.isEmpty());
}

QString DashboardCard::title() const { return titleLabel_->text(); }

QString DashboardCard::status() const { return statusLabel_->text(); }

}  // namespace darkspark::deck::cards
