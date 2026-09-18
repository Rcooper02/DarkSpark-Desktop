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

}  // namespace darkspark::models

#endif  // DARKSPARK_MODELS_CONTROLACTION_HPP
