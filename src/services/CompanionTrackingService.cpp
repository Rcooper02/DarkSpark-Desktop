// SPDX-License-Identifier: GPL-3.0-or-later
#include "services/CompanionTrackingService.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QHostAddress>
#include <QLoggingCategory>
#include <QNetworkDatagram>
#include <QProcess>
#include <QStringList>
#include <QTimer>
#include <QUdpSocket>

namespace darkspark::services {

namespace {
Q_LOGGING_CATEGORY(lcTracking, "darkspark.companion.tracking")
constexpr quint16 kTrackingPort = 45454;
constexpr int kTrackingLostMs = 1200;
constexpr int kTrackerStopTimeoutMs = 1000;
}

CompanionTrackingService::CompanionTrackingService(QObject* parent)
    : QObject(parent),
      socket_(new QUdpSocket(this)),
      lostTimer_(new QTimer(this)),
      trackerProcess_(new QProcess(this)) {
    lostTimer_->setSingleShot(true);
    lostTimer_->setInterval(kTrackingLostMs);

    connect(lostTimer_, &QTimer::timeout,
            this, &CompanionTrackingService::trackingLost);

    connect(socket_, &QUdpSocket::readyRead,
            this, &CompanionTrackingService::receivePendingDatagrams);

    if (!socket_->bind(QHostAddress::LocalHost, kTrackingPort)) {
        qCWarning(lcTracking)
            << "Could not bind HAL tracking receiver:"
            << socket_->errorString();
        return;
    }

    connect(trackerProcess_, &QProcess::readyReadStandardError, this, [this]() {
        const QByteArray message =
            trackerProcess_->readAllStandardError().trimmed();
        if (!message.isEmpty()) {
            qCInfo(lcTracking).noquote() << message;
        }
    });
    connect(
        trackerProcess_,
        qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
        this,
        [](int exitCode, QProcess::ExitStatus status) {
            if (status == QProcess::CrashExit || exitCode != 0) {
                qCWarning(lcTracking)
                    << "HAL face tracker stopped with exit code" << exitCode;
            }
        });

    startTracker();
}

CompanionTrackingService::~CompanionTrackingService() {
    if (trackerProcess_->state() == QProcess::NotRunning) {
        return;
    }

    trackerProcess_->terminate();
    if (!trackerProcess_->waitForFinished(kTrackerStopTimeoutMs)) {
        trackerProcess_->kill();
        trackerProcess_->waitForFinished(kTrackerStopTimeoutMs);
    }
}

void CompanionTrackingService::startTracker() {
    const QDir applicationDir(QCoreApplication::applicationDirPath());
    const QStringList candidates{
        applicationDir.filePath(
            QStringLiteral("../../scripts/hal_face_tracker.py")),
        applicationDir.filePath(
            QStringLiteral("../share/darkspark/hal_face_tracker.py")),
    };

    QString trackerPath;
    for (const QString& candidate : candidates) {
        const QFileInfo tracker(candidate);
        if (tracker.isFile()) {
            trackerPath = tracker.canonicalFilePath();
            break;
        }
    }

    if (trackerPath.isEmpty()) {
        qCWarning(lcTracking)
            << "HAL face tracker helper was not found near"
            << applicationDir.absolutePath();
        return;
    }

    const QString python =
        qEnvironmentVariable("DARKSPARK_PYTHON", QStringLiteral("python3"));
    const QString camera =
        qEnvironmentVariable("DARKSPARK_CAMERA_DEVICE",
                             QStringLiteral("/dev/video0"));

    trackerProcess_->setProgram(python);
    trackerProcess_->setArguments(
        {QStringLiteral("-u"), trackerPath,
         QStringLiteral("--device"), camera,
         QStringLiteral("--port"), QString::number(kTrackingPort)});
    trackerProcess_->start();

    qCInfo(lcTracking)
        << "Starting HAL face tracker for" << camera;
}

void CompanionTrackingService::receivePendingDatagrams() {
    while (socket_->hasPendingDatagrams()) {
        const QNetworkDatagram datagram = socket_->receiveDatagram();
        const QList<QByteArray> fields =
            datagram.data().trimmed().split(' ');

        if (fields.size() != 2) {
            continue;
        }

        bool horizontalOk = false;
        bool verticalOk = false;

        const double horizontal =
            fields.at(0).toDouble(&horizontalOk);
        const double vertical =
            fields.at(1).toDouble(&verticalOk);

        if (!horizontalOk || !verticalOk) {
            continue;
        }

        emit gazeTargetChanged(
            models::GazeTarget{horizontal, vertical}.clamped());

        lostTimer_->start();
    }
}

}  // namespace darkspark::services
