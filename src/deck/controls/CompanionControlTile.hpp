// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_DECK_CONTROLS_COMPANIONCONTROLTILE_HPP
#define DARKSPARK_DECK_CONTROLS_COMPANIONCONTROLTILE_HPP

#include <QPushButton>

#include "deck/controls/CompanionControl.hpp"

namespace darkspark::services {
class CompanionClient;
}

namespace darkspark::deck::controls {

/// Native DarkSpark control tile that delegates action execution to Companion.
/// Press and release are forwarded separately so Companion duration/hold logic
/// keeps working exactly as it does on a physical or web surface.
class CompanionControlTile : public QPushButton {
    Q_OBJECT

public:
    CompanionControlTile(const CompanionControl& control,
                         services::CompanionClient* client,
                         QWidget* parent = nullptr);

    [[nodiscard]] const CompanionControl& control() const { return control_; }

private:
    void refreshPresentation();

    CompanionControl control_;
    services::CompanionClient* client_ = nullptr;
    bool companionAvailable_ = true;
};

}  // namespace darkspark::deck::controls

#endif  // DARKSPARK_DECK_CONTROLS_COMPANIONCONTROLTILE_HPP
