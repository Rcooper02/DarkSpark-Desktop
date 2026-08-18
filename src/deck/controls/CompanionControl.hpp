// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_DECK_CONTROLS_COMPANIONCONTROL_HPP
#define DARKSPARK_DECK_CONTROLS_COMPANIONCONTROL_HPP

#include <QString>

namespace darkspark::deck::controls {

/// Data-only description of a DarkSpark control backed by a Companion grid
/// location. The UI owns presentation; Companion owns the action graph behind
/// the target location.
struct CompanionControl {
    QString label;
    QString subtitle;
    int page = 0;
    int row = 0;
    int column = 0;

    [[nodiscard]] bool isValid() const {
        return !label.trimmed().isEmpty() && page >= 0 && row >= 0
               && column >= 0;
    }
};

}  // namespace darkspark::deck::controls

#endif  // DARKSPARK_DECK_CONTROLS_COMPANIONCONTROL_HPP
