// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_DECK_PAGES_COMPANIONCONTROLSPAGE_HPP
#define DARKSPARK_DECK_PAGES_COMPANIONCONTROLSPAGE_HPP

#include <QWidget>

class QLabel;
class QPushButton;

namespace darkspark::services {
class CompanionClient;
}

namespace darkspark::deck::pages {

/// First native DarkSpark -> Companion integration surface.
///
/// This page is intentionally tiny: one touch-friendly control targets the
/// Companion test location page 1 / row 0 / column 3 using true down/up events.
/// It proves the integration boundary before a configurable control-grid model
/// is introduced.
class CompanionControlsPage : public QWidget {
    Q_OBJECT

public:
    explicit CompanionControlsPage(services::CompanionClient* client,
                                   QWidget* parent = nullptr);

private:
    services::CompanionClient* client_ = nullptr;
    QPushButton* testButton_ = nullptr;
    QLabel* statusLabel_ = nullptr;
};

}  // namespace darkspark::deck::pages

#endif  // DARKSPARK_DECK_PAGES_COMPANIONCONTROLSPAGE_HPP
