// SPDX-License-Identifier: GPL-3.0-or-later
#include "services/CompanionVoiceService.hpp"

#include <QCoreApplication>
#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QProcess>
#include <QStandardPaths>
#include <QUuid>

#include <optional>

namespace darkspark::services {

namespace {

std::optional<models::ControlAction> actionForToken(const QString& token) {
    using models::ControlAction;
    if (token == QStringLiteral("mute")) return ControlAction::Mute;
    if (token == QStringLiteral("unmute")) return ControlAction::Unmute;
    if (token == QStringLiteral("volume_up")) return ControlAction::VolumeUp;
    if (token == QStringLiteral("volume_down")) return ControlAction::VolumeDown;
    if (token == QStringLiteral("play_pause")) return ControlAction::PlayPause;
    if (token == QStringLiteral("next_track")) return ControlAction::NextTrack;
    if (token == QStringLiteral("previous_track")) return ControlAction::PreviousTrack;
    return std::nullopt;
}

QString spokenAlias(QString text) {
    text.replace(QStringLiteral("Starbadger"), QStringLiteral("Star Badger"),
                 Qt::CaseInsensitive);
    return text;
}

}  // namespace

CompanionVoiceService::CompanionVoiceService(QObject* parent) : QObject(parent) {}

void CompanionVoiceService::listen() {
    if (recognitionProcess_ != nullptr || synthesisProcess_ != nullptr ||
        playbackProcess_ != nullptr || awaitingAction_) {
        return;
    }

    const QString python = pythonExecutable();
    const QString helper = helperPath();
    if (python.isEmpty() || helper.isEmpty()) {
        emit errorOccurred(QStringLiteral("HAL speech tools are not available"));
        speak(QStringLiteral("My listening system is not available."));
        return;
    }

    QString source = qEnvironmentVariable("DARKSPARK_AUDIO_SOURCE");
    if (source.isEmpty()) {
        source = QStringLiteral(
            "alsa_input.usb-EMEET_EMEET_PIXY_A260710001805767-02.mono-fallback");
    }

    recognitionProcess_ = new QProcess(this);
    recognitionProcess_->setProcessChannelMode(QProcess::SeparateChannels);
    connect(recognitionProcess_, &QProcess::finished, this,
            &CompanionVoiceService::finishListening);
    connect(recognitionProcess_, &QProcess::errorOccurred, this,
            [this](QProcess::ProcessError error) {
                if (error != QProcess::FailedToStart || recognitionProcess_ == nullptr) {
                    return;
                }
                recognitionProcess_->deleteLater();
                recognitionProcess_ = nullptr;
                emit errorOccurred(QStringLiteral("Could not start HAL listening"));
                emit stateChanged(models::CompanionState::Alert);
            });

    emit stateChanged(models::CompanionState::Listening);
    recognitionProcess_->start(
        python, {helper, QStringLiteral("--source"), source,
                 QStringLiteral("--duration"), QStringLiteral("4.5")});
}

void CompanionVoiceService::finishListening(int exitCode,
                                            QProcess::ExitStatus status) {
    if (recognitionProcess_ == nullptr) return;
    const QByteArray output = recognitionProcess_->readAllStandardOutput();
    const QString diagnostics =
        QString::fromUtf8(recognitionProcess_->readAllStandardError()).simplified();
    recognitionProcess_->deleteLater();
    recognitionProcess_ = nullptr;

    if (status != QProcess::NormalExit || exitCode != 0) {
        emit errorOccurred(diagnostics.isEmpty()
                               ? QStringLiteral("HAL could not understand the microphone")
                               : diagnostics.left(240));
        speak(QStringLiteral("I could not hear you clearly."));
        return;
    }

    // Native whisper diagnostics normally use stderr, but keep parsing robust
    // if a backend writes a banner to stdout: the helper's JSON is its final
    // non-empty line.
    const QList<QByteArray> outputLines = output.trimmed().split('\n');
    const QByteArray jsonLine = outputLines.isEmpty() ? QByteArray() : outputLines.last();
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(jsonLine, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        emit errorOccurred(QStringLiteral("HAL received an invalid speech result"));
        speak(QStringLiteral("I could not interpret that command."));
        return;
    }

    const QJsonObject result = document.object();
    const QString transcript = result.value(QStringLiteral("transcript")).toString();
    const QString normalized = result.value(QStringLiteral("normalized")).toString();
    const QString token = result.value(QStringLiteral("command")).toString();
    const QString response = result.value(QStringLiteral("response")).toString();
    emit transcriptReady(transcript);
    emit stateChanged(models::CompanionState::Thinking);

    const std::optional<models::ControlAction> action = actionForToken(token);
    if (!action.has_value()) {
        if (normalized.isEmpty()) {
            speak(response.isEmpty() ? QStringLiteral("I heard only silence.")
                                     : response);
        } else {
            emit conversationRequested(normalized);
        }
        return;
    }

    awaitingAction_ = true;
    pendingAction_ = *action;
    pendingResponse_ = response;
    emit controlRequested(*action);
}

void CompanionVoiceService::speakResponse(const QString& response) {
    speak(response.simplified().left(900));
}

void CompanionVoiceService::handleConversationError(const QString& message) {
    emit errorOccurred(message);
    speak(QStringLiteral("My conversational system is not available."));
}

void CompanionVoiceService::handleActionCompleted(models::ControlAction action,
                                                   bool success,
                                                   const QString& message) {
    if (!awaitingAction_ || action != pendingAction_) return;
    awaitingAction_ = false;
    const QString response =
        success ? pendingResponse_
                : QStringLiteral("I was unable to complete that command. %1").arg(message);
    pendingResponse_.clear();
    speak(response);
}

void CompanionVoiceService::speak(const QString& text) {
    const QString python = pythonExecutable();
    const QString model = voiceModelPath();
    if (python.isEmpty() || !QFileInfo::exists(model)) {
        emit errorOccurred(QStringLiteral("HAL voice model was not found at %1").arg(model));
        emit stateChanged(models::CompanionState::Idle);
        return;
    }

    speechFilePath_ = QDir(QDir::tempPath()).filePath(
        QStringLiteral("darkspark-hal-%1.wav")
            .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
    synthesisProcess_ = new QProcess(this);
    connect(synthesisProcess_, &QProcess::finished, this,
            [this](int exitCode, QProcess::ExitStatus status) {
                synthesisProcess_->deleteLater();
                synthesisProcess_ = nullptr;
                if (status != QProcess::NormalExit || exitCode != 0) {
                    emit errorOccurred(QStringLiteral("HAL voice synthesis failed"));
                    finishSpeaking();
                    return;
                }
                const QString player = QStandardPaths::findExecutable(
                    QStringLiteral("pw-play"));
                if (player.isEmpty()) {
                    emit errorOccurred(QStringLiteral("pw-play is not installed"));
                    finishSpeaking();
                    return;
                }
                playbackProcess_ = new QProcess(this);
                connect(playbackProcess_, &QProcess::finished, this,
                        [this](int, QProcess::ExitStatus) {
                            playbackProcess_->deleteLater();
                            playbackProcess_ = nullptr;
                            finishSpeaking();
                        });
                playbackProcess_->start(player, {speechFilePath_});
            });

    emit stateChanged(models::CompanionState::Speaking);
    synthesisProcess_->start(
        python,
        {QStringLiteral("-m"), QStringLiteral("piper"), QStringLiteral("--model"),
         model, QStringLiteral("--output-file"), speechFilePath_,
         QStringLiteral("--"), spokenAlias(text)});
}

void CompanionVoiceService::finishSpeaking() {
    if (!speechFilePath_.isEmpty()) QFile::remove(speechFilePath_);
    speechFilePath_.clear();
    emit stateChanged(models::CompanionState::Idle);
}

QString CompanionVoiceService::pythonExecutable() const {
    const QString configured = qEnvironmentVariable("DARKSPARK_VOICE_PYTHON");
    if (!configured.isEmpty() && QFileInfo::exists(configured)) return configured;

    const QDir appDir(QCoreApplication::applicationDirPath());
    const QString projectVenv = appDir.absoluteFilePath(QStringLiteral("../../.venv/bin/python"));
    if (QFileInfo::exists(projectVenv)) return QFileInfo(projectVenv).canonicalFilePath();
    const QString workingVenv = QDir::current().absoluteFilePath(QStringLiteral(".venv/bin/python"));
    if (QFileInfo::exists(workingVenv)) return QFileInfo(workingVenv).canonicalFilePath();
    return QStandardPaths::findExecutable(QStringLiteral("python3"));
}

QString CompanionVoiceService::helperPath() const {
    const QString configured = qEnvironmentVariable("DARKSPARK_VOICE_HELPER");
    if (!configured.isEmpty() && QFileInfo::exists(configured)) return configured;

    const QDir appDir(QCoreApplication::applicationDirPath());
    const QString buildTree = appDir.absoluteFilePath(QStringLiteral("../../scripts/hal_voice_command.py"));
    if (QFileInfo::exists(buildTree)) return QFileInfo(buildTree).canonicalFilePath();
    const QString installed = appDir.absoluteFilePath(QStringLiteral("../share/darkspark/hal_voice_command.py"));
    if (QFileInfo::exists(installed)) return QFileInfo(installed).canonicalFilePath();
    return {};
}

QString CompanionVoiceService::voiceModelPath() const {
    const QString configured = qEnvironmentVariable("DARKSPARK_HAL_VOICE_MODEL");
    if (!configured.isEmpty()) return configured;
    return QDir::home().filePath(
        QStringLiteral(".local/share/darkspark/voices/hal9000/hal.onnx"));
}

}  // namespace darkspark::services
