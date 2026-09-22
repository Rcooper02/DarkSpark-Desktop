// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_MODELS_CONTROLACTION_HPP
#define DARKSPARK_MODELS_CONTROLACTION_HPP

namespace darkspark::models {

/// Fixed, allow-listed desktop and audio actions exposed by Control Deck UI.
enum class ControlAction {
    LaunchFirefox,
    LaunchSteam,
    LaunchTerminal,
    LaunchFiles,
    LaunchSystemMonitor,
    LockSession,
    PreviousTrack,
    PlayPause,
    NextTrack,
    VolumeDown,
    ToggleMute,
    VolumeUp
};

[[nodiscard]] constexpr bool isAudioAction(ControlAction action) {
    switch (action) {
    case ControlAction::PreviousTrack:
    case ControlAction::PlayPause:
    case ControlAction::NextTrack:
    case ControlAction::VolumeDown:
    case ControlAction::ToggleMute:
    case ControlAction::VolumeUp:
        return true;
    case ControlAction::LaunchFirefox:
    case ControlAction::LaunchSteam:
    case ControlAction::LaunchTerminal:
    case ControlAction::LaunchFiles:
    case ControlAction::LaunchSystemMonitor:
    case ControlAction::LockSession:
        return false;
    }
    return false;
}

}  // namespace darkspark::models

#endif  // DARKSPARK_MODELS_CONTROLACTION_HPP
