// SPDX-License-Identifier: GPL-3.0-or-later
#include "services/CompanionListeningService.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QProcess>
#include <QTimer>

namespace darkspark::services {

namespace {
Q_LOGGING_CATEGORY(lcListening, "darkspark.companion.listening")

constexpr int kRecordingMilliseconds = 6000;

const QString kDefaultMicrophone =
    QStringLiteral(
        "alsa_input.usb-Generic_Razer_Seiren_V3_Chroma_"
        "UC2425L07500462-00.analog-stereo");
}

CompanionListeningService::CompanionListeningService(QObject* parent)
    : QObject(parent),
      recordProcess_(new QProcess(this)),
      whisperProcess_(new QProcess(this)),
      recordTimer_(new QTimer(this)),
      recordingPath_(QStringLiteral("/tmp/darkspark-hal-listen.wav")) {
    recordTimer_->setSingleShot(true);
    connect(whisperProcess_, &QProcess::errorOccurred, this,
            [this](QProcess::ProcessError error) {
                if (error == QProcess::FailedToStart)
                    emit listeningFailed(QStringLiteral("Unable to start Whisper: %1")
                                             .arg(whisperProcess_->errorString()));
            });

    connect(recordTimer_, &QTimer::timeout,
            this, &CompanionListeningService::stopRecordingAndTranscribe);

    connect(
        whisperProcess_,
        qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
        this,
        [this](int exitCode, QProcess::ExitStatus status) {
            const QString stderrText =
                QString::fromUtf8(
                    whisperProcess_->readAllStandardError());

            if (status != QProcess::NormalExit || exitCode != 0) {
                qCWarning(lcListening).noquote()
                    << "Whisper failed:" << stderrText.trimmed();

                emit listeningFailed(
                    QStringLiteral("Whisper failed: %1")
                        .arg(stderrText.trimmed()));
                return;
            }

            const QString text =
                QString::fromUtf8(
                    whisperProcess_->readAllStandardOutput()).trimmed();

            qCInfo(lcListening).noquote()
                << "HAL heard:" << text;

            emit transcriptionReady(text);
        });
}

CompanionListeningService::~CompanionListeningService() {
    stop();
}

QString CompanionListeningService::findWhisperPython() const {
    const QString override =
        qEnvironmentVariable("DARKSPARK_WHISPER_PYTHON");

    if (!override.isEmpty() && QFileInfo::exists(override)) {
        return override;
    }

    const QDir appDir(QCoreApplication::applicationDirPath());

    const QStringList candidates{
        appDir.filePath(QStringLiteral("../../.venv-whisper/bin/python")),
        appDir.filePath(QStringLiteral("../.venv-whisper/bin/python"))
    };

    for (const QString& candidate : candidates) {
        if (QFileInfo::exists(candidate)) {
            // Preserve the virtual-environment launcher path. Resolving this
            // symlink to /usr/bin/python would bypass the venv site-packages.
            return QFileInfo(candidate).absoluteFilePath();
        }
    }

    return {};
}

void CompanionListeningService::listen() {
    if (recordProcess_->state() != QProcess::NotRunning ||
        whisperProcess_->state() != QProcess::NotRunning) {
        qCWarning(lcListening) << "HAL listening request ignored: busy";
        return;
    }

    recordProcess_->setProgram(QStringLiteral("/usr/bin/parec"));
    recordProcess_->setArguments({
        QStringLiteral("--device=%1").arg(kDefaultMicrophone),
        QStringLiteral("--format=s16le"),
        QStringLiteral("--rate=16000"),
        QStringLiteral("--channels=1"),
        QStringLiteral("--file-format=wav")
    });

    recordProcess_->setStandardOutputFile(recordingPath_,
                                          QIODevice::Truncate);

    qCInfo(lcListening) << "HAL listening on Razer Seiren";
    emit listeningStarted();

    recordProcess_->start();

    if (!recordProcess_->waitForStarted(1500)) {
        emit listeningFailed(
            QStringLiteral("Unable to start microphone recording: %1")
                .arg(recordProcess_->errorString()));
        return;
    }

    recordTimer_->start(kRecordingMilliseconds);
}

void CompanionListeningService::stopRecordingAndTranscribe() {
    if (recordProcess_->state() != QProcess::NotRunning) {
        recordProcess_->terminate();

        if (!recordProcess_->waitForFinished(1000)) {
            recordProcess_->kill();
            recordProcess_->waitForFinished(1000);
        }
    }

    const QString python = findWhisperPython();

    if (python.isEmpty()) {
        emit listeningFailed(
            QStringLiteral("Whisper Python environment not found"));
        return;
    }

    emit transcriptionStarted();

    const QString script = QStringLiteral(R"PY(
from pywhispercpp.model import Model
import sys

path = sys.argv[1]

model = Model(
    "base.en",
    n_threads=6,
    print_progress=False,
    print_realtime=False,
)

segments = model.transcribe(path)
text = " ".join(segment.text.strip() for segment in segments).strip()
print(text)
)PY");

    whisperProcess_->setProgram(python);
    whisperProcess_->setArguments({
        QStringLiteral("-c"),
        script,
        recordingPath_
    });

    whisperProcess_->setProcessChannelMode(QProcess::SeparateChannels);

    qCInfo(lcListening) << "HAL transcribing";
    whisperProcess_->start();
}

void CompanionListeningService::stop() {
    recordTimer_->stop();

    for (QProcess* process : {recordProcess_, whisperProcess_}) {
        if (process->state() == QProcess::NotRunning) {
            continue;
        }

        process->terminate();

        if (!process->waitForFinished(1000)) {
            process->kill();
            process->waitForFinished(1000);
        }
    }
}

}  // namespace darkspark::services
