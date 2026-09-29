// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_SERVICES_COMPANIONVOICESERVICE_HPP
#define DARKSPARK_SERVICES_COMPANIONVOICESERVICE_HPP

#include "models/CompanionState.hpp"
#include "models/ControlAction.hpp"

#include <QObject>
#include <QProcess>
#include <QString>

namespace darkspark::services {

/// Coordinates local speech recognition, allow-listed controls, and HAL TTS.
///
/// Speech never becomes a shell command. The Python recognizer returns a fixed
/// command token, which is independently mapped to ControlAction here.
class CompanionVoiceService final : public QObject {
    Q_OBJECT

public:
    explicit CompanionVoiceService(QObject* parent = nullptr);

public slots:
    void listen();
    void speakResponse(const QString& response);
    void handleConversationError(const QString& message);
    void handleActionCompleted(models::ControlAction action, bool success,
                               const QString& message);

signals:
    void stateChanged(models::CompanionState state);
    void controlRequested(models::ControlAction action);
    void transcriptReady(const QString& transcript);
    void conversationRequested(const QString& transcript);
    void errorOccurred(const QString& message);

private:
    void finishListening(int exitCode, QProcess::ExitStatus status);
    void speak(const QString& text);
    void finishSpeaking();
    [[nodiscard]] QString pythonExecutable() const;
    [[nodiscard]] QString helperPath() const;
    [[nodiscard]] QString voiceModelPath() const;

    QProcess* recognitionProcess_ = nullptr;
    QProcess* synthesisProcess_ = nullptr;
    QProcess* playbackProcess_ = nullptr;
    bool awaitingAction_ = false;
    models::ControlAction pendingAction_ = models::ControlAction::PlayPause;
    QString pendingResponse_;
    QString speechFilePath_;
};

}  // namespace darkspark::services

#endif  // DARKSPARK_SERVICES_COMPANIONVOICESERVICE_HPP
