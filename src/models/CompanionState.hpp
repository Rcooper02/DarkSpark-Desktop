// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_MODELS_COMPANIONSTATE_HPP
#define DARKSPARK_MODELS_COMPANIONSTATE_HPP

#include <string_view>

namespace darkspark::models {

/// Observable interaction state of the DarkSpark Companion.
///
/// This is intentionally data-only. Camera, audio, speech, and AI services may
/// publish this state later without depending on any Deck presentation type.
enum class CompanionState {
    Dormant,
    Idle,
    Listening,
    Thinking,
    Speaking,
    Alert
};

[[nodiscard]] constexpr std::string_view companionStateName(CompanionState state) {
    switch (state) {
    case CompanionState::Dormant:
        return "Dormant";
    case CompanionState::Idle:
        return "Idle";
    case CompanionState::Listening:
        return "Listening";
    case CompanionState::Thinking:
        return "Thinking";
    case CompanionState::Speaking:
        return "Speaking";
    case CompanionState::Alert:
        return "Alert";
    }
    return "Idle";
}

}  // namespace darkspark::models

#endif  // DARKSPARK_MODELS_COMPANIONSTATE_HPP
