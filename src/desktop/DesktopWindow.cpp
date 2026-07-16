// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/DesktopWindow.hpp"

#include "themes/LegacyTheme.hpp"

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace darkspark::desktop {

using themes::LegacyTheme;

DesktopWindow::DesktopWindow(QWidget* parent) : QWidget(parent) {
    setWindowTitle(QStringLiteral("DarkSpark Desktop"));
    resize(960, 600);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(LegacyTheme::spaceXl(), LegacyTheme::spaceXl(),
                             LegacyTheme::spaceXl(), LegacyTheme::spaceXl());
    root->setSpacing(LegacyTheme::spaceLg());
    root->addStretch(1);

    auto* title = new QLabel(QStringLiteral("DarkSpark Desktop"), this);
    title->setProperty("legacyRole", "pageTitle");
    title->setAlignment(Qt::AlignCenter);

    auto* motto = new QLabel(QStringLiteral("Knowledge Belongs to All."), this);
    motto->setProperty("legacyRole", "cardStatus");
    motto->setAlignment(Qt::AlignCenter);

    auto* conceptLabel = new QLabel(QStringLiteral("Desktop Command Center"), this);
    conceptLabel->setProperty("legacyRole", "cardTitle");
    conceptLabel->setAlignment(Qt::AlignCenter);

    auto* launch = new QPushButton(QStringLiteral("Launch Deck Mode"), this);
    launch->setMinimumSize(LegacyTheme::touchTargetMin() * 4,
                           LegacyTheme::touchTargetMin());
    launch->setCursor(Qt::PointingHandCursor);
    connect(launch, &QPushButton::clicked, this, &DesktopWindow::launchDeckRequested);

    root->addWidget(title);
    root->addWidget(conceptLabel);
    root->addWidget(motto);
    root->addSpacing(LegacyTheme::spaceXl());
    root->addWidget(launch, 0, Qt::AlignCenter);
    root->addStretch(2);
}

}  // namespace darkspark::desktop
