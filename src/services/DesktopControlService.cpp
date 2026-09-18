// SPDX-License-Identifier: GPL-3.0-or-later
#include "services/DesktopControlService.hpp"

#include <QDir>
#include <QProcess>
#include <QStandardPaths>

namespace darkspark::services {

DesktopControlService::DesktopControlService(QObject* parent) : QObject(parent) {}

void DesktopControlService::perform(models::ControlAction action) {
    using models::ControlAction;
    switch (action) {
    case ControlAction::LaunchFirefox:
        launch(action, QStringLiteral("firefox"), {}, QStringLiteral("Firefox launched"));
        return;
    case ControlAction::LaunchSteam:
        launch(action, QStringLiteral("steam"), {}, QStringLiteral("Steam launched"));
        return;
    case ControlAction::LaunchTerminal:
        launch(action, QStringLiteral("xdg-terminal-exec"), {},
               QStringLiteral("Terminal launched"));
        return;
    case ControlAction::LaunchFiles:
        launch(action, QStringLiteral("xdg-open"), {QDir::homePath()},
               QStringLiteral("Files opened"));
        return;
    case ControlAction::LaunchSystemMonitor:
        launch(action, QStringLiteral("plasma-systemmonitor"), {},
               QStringLiteral("System Monitor launched"));
        return;
    case ControlAction::LockSession:
        launch(action, QStringLiteral("loginctl"), {QStringLiteral("lock-session")},
               QStringLiteral("Session lock requested"));
        return;
    case ControlAction::PreviousTrack:
        launch(action, QStringLiteral("playerctl"), {QStringLiteral("previous")},
               QStringLiteral("Previous track"));
        return;
    case ControlAction::PlayPause:
        launch(action, QStringLiteral("playerctl"), {QStringLiteral("play-pause")},
               QStringLiteral("Playback toggled"));
        return;
    case ControlAction::NextTrack:
        launch(action, QStringLiteral("playerctl"), {QStringLiteral("next")},
               QStringLiteral("Next track"));
        return;
    case ControlAction::VolumeDown:
        launch(action, QStringLiteral("wpctl"),
               {QStringLiteral("set-volume"), QStringLiteral("@DEFAULT_AUDIO_SINK@"),
                QStringLiteral("5%-")},
               QStringLiteral("Volume decreased"));
        return;
    case ControlAction::ToggleMute:
        launch(action, QStringLiteral("wpctl"),
               {QStringLiteral("set-mute"), QStringLiteral("@DEFAULT_AUDIO_SINK@"),
                QStringLiteral("toggle")},
               QStringLiteral("Mute toggled"));
        return;
    case ControlAction::VolumeUp:
        launch(action, QStringLiteral("wpctl"),
               {QStringLiteral("set-volume"), QStringLiteral("@DEFAULT_AUDIO_SINK@"),
                QStringLiteral("5%+")},
               QStringLiteral("Volume increased"));
        return;
    }
}

void DesktopControlService::launch(models::ControlAction action,
                                   const QString& program,
                                   const QStringList& arguments,
                                   const QString& successMessage) {
    const QString executable = QStandardPaths::findExecutable(program);
    if (executable.isEmpty()) {
        emit actionCompleted(action, false,
                             QStringLiteral("%1 is not installed").arg(program));
        return;
    }
    const bool started = QProcess::startDetached(executable, arguments);
    emit actionCompleted(action, started,
                         started ? successMessage
                                 : QStringLiteral("Could not start %1").arg(program));
}

}  // namespace darkspark::services
