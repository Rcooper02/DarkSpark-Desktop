// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_DECK_PAGES_COMPANIONCONTROLSPAGE_HPP
#define DARKSPARK_DECK_PAGES_COMPANIONCONTROLSPAGE_HPP

#include <QWidget>

class QLabel;

namespace darkspark::services {
class CompanionClient;
}

namespace darkspark::deck::pages {

/// Native DarkSpark Controls page backed by Bitfocus Companion.
///
/// DarkSpark owns the visual grid and touch interaction. Companion remains a
/// separate action engine addressed by page/row/column targets.
class CompanionControlsPage : public QWidget {
    Q_OBJECT

public:
    explicit CompanionControlsPage(services::CompanionClient* client,
                                   QWidget* parent = nullptr);

private:
    services::CompanionClient* client_ = nullptr;
    QLabel* statusLabel_ = nullptr;
};

}  // namespace darkspark::deck::pages

#endif  // DARKSPARK_DECK_PAGES_COMPANIONCONTROLSPAGE_HPP
