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
        launchDetached(action, QStringLiteral("firefox"), {},
                       QStringLiteral("Firefox launched"));
        return;
    case ControlAction::LaunchSteam:
        launchDetached(action, QStringLiteral("steam"), {},
                       QStringLiteral("Steam launched"));
        return;
    case ControlAction::LaunchTerminal:
        launchDetached(action, QStringLiteral("xdg-terminal-exec"), {},
                       QStringLiteral("Terminal launched"));
        return;
    case ControlAction::LaunchFiles:
        launchDetached(action, QStringLiteral("xdg-open"), {QDir::homePath()},
                       QStringLiteral("Files opened"));
        return;
    case ControlAction::LaunchSystemMonitor:
        launchDetached(action, QStringLiteral("plasma-systemmonitor"), {},
                       QStringLiteral("System Monitor launched"));
        return;
    case ControlAction::LockSession:
        runCommand(action, QStringLiteral("loginctl"),
                   {QStringLiteral("lock-session")},
                   QStringLiteral("Session lock requested"));
        return;
    case ControlAction::PreviousTrack:
        runCommand(action, QStringLiteral("playerctl"),
                   {QStringLiteral("previous")}, QStringLiteral("Previous track"));
        return;
    case ControlAction::PlayPause:
        runCommand(action, QStringLiteral("playerctl"),
                   {QStringLiteral("play-pause")},
                   QStringLiteral("Playback toggled"));
        return;
    case ControlAction::NextTrack:
        runCommand(action, QStringLiteral("playerctl"), {QStringLiteral("next")},
                   QStringLiteral("Next track"));
        return;
    case ControlAction::VolumeDown:
        runCommand(action, QStringLiteral("wpctl"),
                   {QStringLiteral("set-volume"),
                    QStringLiteral("@DEFAULT_AUDIO_SINK@"),
                    QStringLiteral("5%-")},
                   QStringLiteral("Volume decreased"));
        return;
    case ControlAction::ToggleMute:
        runCommand(action, QStringLiteral("wpctl"),
                   {QStringLiteral("set-mute"),
                    QStringLiteral("@DEFAULT_AUDIO_SINK@"),
                    QStringLiteral("toggle")},
                   QStringLiteral("Mute toggled"));
        return;
    case ControlAction::VolumeUp:
        runCommand(action, QStringLiteral("wpctl"),
                   {QStringLiteral("set-volume"),
                    QStringLiteral("--limit"), QStringLiteral("1.0"),
                    QStringLiteral("@DEFAULT_AUDIO_SINK@"),
                    QStringLiteral("5%+")},
                   QStringLiteral("Volume increased"));
        return;
    }
}

void DesktopControlService::launchDetached(models::ControlAction action,
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

void DesktopControlService::runCommand(models::ControlAction action,
                                       const QString& program,
                                       const QStringList& arguments,
                                       const QString& successMessage) {
    const QString executable = QStandardPaths::findExecutable(program);
    if (executable.isEmpty()) {
        emit actionCompleted(action, false,
                             QStringLiteral("%1 is not installed").arg(program));
        return;
    }

    auto* process = new QProcess(this);
    connect(process, &QProcess::finished, this,
            [this, process, action, successMessage, program](int exitCode,
                                                             QProcess::ExitStatus status) {
                if (process->property("completionReported").toBool()) {
                    return;
                }
                process->setProperty("completionReported", true);
                const bool succeeded = status == QProcess::NormalExit && exitCode == 0;
                QString message = successMessage;
                if (!succeeded) {
                    const QString error = QString::fromUtf8(process->readAllStandardError())
                                              .simplified()
                                              .left(160);
                    message = error.isEmpty()
                                  ? QStringLiteral("%1 failed (exit %2)")
                                        .arg(program)
                                        .arg(exitCode)
                                  : error;
                }
                emit actionCompleted(action, succeeded, message);
                process->deleteLater();
            });
    connect(process, &QProcess::errorOccurred, this,
            [this, process, action, program](QProcess::ProcessError error) {
                if (error != QProcess::FailedToStart) {
                    return;
                }
                if (process->property("completionReported").toBool()) {
                    return;
                }
                process->setProperty("completionReported", true);
                emit actionCompleted(action, false,
                                     QStringLiteral("Could not start %1").arg(program));
                process->deleteLater();
            });
    process->start(executable, arguments);
}

}  // namespace darkspark::services
