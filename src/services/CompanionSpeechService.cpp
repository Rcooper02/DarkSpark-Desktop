// SPDX-License-Identifier: GPL-3.0-or-later
#include "services/CompanionSpeechService.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QProcess>
#include <QStringList>

namespace darkspark::services {

namespace {
Q_LOGGING_CATEGORY(lcSpeech, "darkspark.companion.speech")
constexpr int kStopTimeoutMs = 1000;
}

CompanionSpeechService::CompanionSpeechService(QObject* parent)
    : QObject(parent),
      piperProcess_(new QProcess(this)),
      playbackProcess_(new QProcess(this)),
      outputPath_(QDir::temp().filePath(QStringLiteral("darkspark-hal.wav"))) {

    connect(
        piperProcess_,
        qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
        this,
        [this](int exitCode, QProcess::ExitStatus status) {
            if (status != QProcess::NormalExit || exitCode != 0) {
                const QString error =
                    QString::fromUtf8(piperProcess_->readAllStandardError()).trimmed();
                qCWarning(lcSpeech) << "Piper failed:" << error;
                emit speechFailed(
                    error.isEmpty() ? QStringLiteral("Piper synthesis failed")
                                    : error);
                return;
            }

            startPlayback();
        });

    connect(
        playbackProcess_,
        qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
        this,
        [this](int exitCode, QProcess::ExitStatus status) {
            if (status != QProcess::NormalExit || exitCode != 0) {
                const QString error =
                    QString::fromUtf8(playbackProcess_->readAllStandardError())
                        .trimmed();
                qCWarning(lcSpeech) << "HAL playback failed:" << error;
                emit speechFailed(
                    error.isEmpty() ? QStringLiteral("HAL playback failed")
                                    : error);
                return;
            }

            emit speechFinished();
        });
}

CompanionSpeechService::~CompanionSpeechService() {
    stop();
}

QString CompanionSpeechService::findPiper() const {
    const QString override = qEnvironmentVariable("DARKSPARK_PIPER");
    if (!override.isEmpty() && QFileInfo::exists(override)) {
        return override;
    }

    const QDir applicationDir(QCoreApplication::applicationDirPath());
    const QStringList candidates{
        applicationDir.filePath(QStringLiteral("../../.venv-piper/bin/piper")),
        applicationDir.filePath(QStringLiteral("../.venv-piper/bin/piper")),
    };

    for (const QString& candidate : candidates) {
        const QFileInfo info(candidate);
        if (info.isExecutable()) {
            return info.canonicalFilePath();
        }
    }

    return {};
}

QString CompanionSpeechService::findModel() const {
    const QString override = qEnvironmentVariable("DARKSPARK_HAL_MODEL");
    if (!override.isEmpty() && QFileInfo::exists(override)) {
        return override;
    }

    const QDir applicationDir(QCoreApplication::applicationDirPath());
    const QStringList candidates{
        applicationDir.filePath(
            QStringLiteral("../../models/hal-piper/hal.onnx")),
        applicationDir.filePath(
            QStringLiteral("../models/hal-piper/hal.onnx")),
    };

    for (const QString& candidate : candidates) {
        const QFileInfo info(candidate);
        if (info.isFile()) {
            return info.canonicalFilePath();
        }
    }

    return {};
}

void CompanionSpeechService::speak(const QString& text) {
    if (text.trimmed().isEmpty()) {
        return;
    }

    stop();

    const QString piper = findPiper();
    const QString model = findModel();

    if (piper.isEmpty()) {
        emit speechFailed(QStringLiteral("Piper executable not found"));
        return;
    }

    if (model.isEmpty()) {
        emit speechFailed(QStringLiteral("HAL Piper model not found"));
        return;
    }

    QFile::remove(outputPath_);

    piperProcess_->setProgram(piper);
    piperProcess_->setArguments({
        QStringLiteral("--model"),
        model,
        QStringLiteral("--output_file"),
        outputPath_,
        QStringLiteral("--length-scale"),
        QStringLiteral("1.12"),
    });

    piperProcess_->start();

    if (!piperProcess_->waitForStarted(1000)) {
        emit speechFailed(QStringLiteral("Could not start Piper"));
        return;
    }

    piperProcess_->write(text.toUtf8());
    piperProcess_->write("\n");
    piperProcess_->closeWriteChannel();

    qCInfo(lcSpeech) << "HAL speech synthesis started";
    emit speechStarted();
}

void CompanionSpeechService::startPlayback() {
    if (!QFileInfo::exists(outputPath_)) {
        emit speechFailed(QStringLiteral("Piper produced no WAV file"));
        return;
    }

    playbackProcess_->setProgram(QStringLiteral("paplay"));
    playbackProcess_->setArguments({outputPath_});
    playbackProcess_->start();

    if (!playbackProcess_->waitForStarted(1000)) {
        emit speechFailed(QStringLiteral("Could not start paplay"));
    }
}

void CompanionSpeechService::stop() {
    for (QProcess* process : {playbackProcess_, piperProcess_}) {
        if (process->state() == QProcess::NotRunning) {
            continue;
        }

        process->terminate();
        if (!process->waitForFinished(kStopTimeoutMs)) {
            process->kill();
            process->waitForFinished(kStopTimeoutMs);
        }
    }
}

}  // namespace darkspark::services
