// SPDX-License-Identifier: GPL-3.0-or-later
#include "services/CompanionListeningService.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QProcess>
#include <QTimer>
#include <QStringList>

namespace darkspark::services {
namespace {
Q_LOGGING_CATEGORY(lcListening, "darkspark.companion.listening")
}
CompanionListeningService::CompanionListeningService(QObject* parent)
    : QObject(parent), helper_(new QProcess(this)) {
    helper_->setProcessChannelMode(QProcess::SeparateChannels);
    connect(helper_, &QProcess::readyReadStandardError, this, [this]() {
        const QByteArray diagnostics = helper_->readAllStandardError();
        const QList<QByteArray> lines = diagnostics.split('\n');
        for (const QByteArray& line : lines) {
            if (line.contains("[wake-debug]"))
                qCInfo(lcListening).noquote() << line;
        }
    });
    connect(helper_, &QProcess::readyReadStandardOutput, this, [this]() {
        output_.append(helper_->readAllStandardOutput());
        qsizetype end;
        while ((end = output_.indexOf('\n')) >= 0) {
            const QByteArray line = output_.left(end);
            output_.remove(0, end + 1);
            const auto object = QJsonDocument::fromJson(line).object();
            const QString event = object.value(QStringLiteral("event")).toString();
            if (event == QStringLiteral("wake_detected")) {
                qCInfo(lcListening) << "HAL wake word detected";
                emit wakeDetected();
            }
            else if (event == QStringLiteral("listening")) emit listeningStarted();
            else if (event == QStringLiteral("transcribing")) emit transcriptionStarted();
            else if (event == QStringLiteral("transcript")) {
                const QString text = object.value(QStringLiteral("text")).toString();
                qCInfo(lcListening) << "HAL heard:" << text;
                emit transcriptionReady(text);
            } else if (event == QStringLiteral("error")) {
                emit listeningFailed(object.value(QStringLiteral("message")).toString());
            } else if (event == QStringLiteral("ready")) {
                qCInfo(lcListening) << "HAL microphone ready; wake listening:"
                    << object.value(QStringLiteral("wake")).toBool();
            }
        }
    });
    connect(helper_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (!stopping_ && error == QProcess::FailedToStart)
            emit listeningFailed(helper_->errorString());
    });
    connect(helper_, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, [this](int, QProcess::ExitStatus) {
                if (!stopping_) emit listeningFailed(QStringLiteral("HAL microphone helper stopped"));
            });
    if (qEnvironmentVariable("DARKSPARK_HAL_WAKE") == QStringLiteral("1"))
        QTimer::singleShot(3000, this, [this]() { startHelper(); });
}
CompanionListeningService::~CompanionListeningService() { stop(); }

QString CompanionListeningService::findWhisperPython() const {
    const QString override = qEnvironmentVariable("DARKSPARK_WHISPER_PYTHON");
    if (!override.isEmpty() && QFileInfo::exists(override)) return override;
    const QDir appDir(QCoreApplication::applicationDirPath());
    for (const QString& relative : {QStringLiteral("../../.venv-whisper/bin/python"),
                                   QStringLiteral("../.venv-whisper/bin/python")}) {
        const QFileInfo candidate(appDir.filePath(relative));
        if (candidate.exists()) return candidate.absoluteFilePath();
    }
    return {};
}

bool CompanionListeningService::startHelper() {
    if (helper_->state() != QProcess::NotRunning) return true;
    const QString python = findWhisperPython();
    const QDir appDir(QCoreApplication::applicationDirPath());
    QString script;
    for (const QString& relative : {QStringLiteral("../../scripts/hal_listen.py"),
                                   QStringLiteral("../scripts/hal_listen.py")}) {
        const QFileInfo candidate(appDir.filePath(relative));
        if (candidate.isFile()) { script = candidate.absoluteFilePath(); break; }
    }
    if (python.isEmpty() || script.isEmpty()) {
        emit listeningFailed(QStringLiteral("HAL Whisper environment or hal_listen.py not found"));
        return false;
    }
    QString microphone = qEnvironmentVariable("DARKSPARK_MICROPHONE");
    if (microphone.isEmpty()) microphone = QStringLiteral("all");
    QStringList args{QStringLiteral("-u"), script, QStringLiteral("--device"), microphone};
    if (qEnvironmentVariable("DARKSPARK_HAL_WAKE") == QStringLiteral("1"))
        args.append(QStringLiteral("--wake"));
    output_.clear(); stopping_ = false;
    helper_->start(python, args);
    if (!helper_->waitForStarted(1000)) return false;
    if (paused_) helper_->write("pause\n");
    return true;
}
void CompanionListeningService::listen() {
    if (!paused_ && startHelper()) helper_->write("listen\n");
}
void CompanionListeningService::setPaused(bool paused) {
    qCInfo(lcListening) << "HAL listening pause state ->" << paused;
    paused_ = paused;
    if (helper_->state() != QProcess::NotRunning)
        helper_->write(paused ? "pause\n" : "resume\n");
}
void CompanionListeningService::stop() {
    stopping_ = true;
    if (helper_->state() == QProcess::NotRunning) return;
    helper_->write("stop\n");
    helper_->closeWriteChannel();
    if (helper_->state() != QProcess::NotRunning && !helper_->waitForFinished(1500)) {
        helper_->terminate();
        if (!helper_->waitForFinished(1000)) { helper_->kill(); helper_->waitForFinished(1000); }
    }
}
}
