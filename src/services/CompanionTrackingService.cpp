// SPDX-License-Identifier: GPL-3.0-or-later
#include "services/CompanionTrackingService.hpp"

#include <QHostAddress>
#include <QNetworkDatagram>
#include <QTimer>
#include <QUdpSocket>

namespace darkspark::services {

namespace {
constexpr quint16 kTrackingPort = 45454;
constexpr int kTrackingLostMs = 1200;
}

CompanionTrackingService::CompanionTrackingService(QObject* parent)
    : QObject(parent),
      socket_(new QUdpSocket(this)),
      lostTimer_(new QTimer(this)) {
    lostTimer_->setSingleShot(true);
    lostTimer_->setInterval(kTrackingLostMs);

    connect(lostTimer_, &QTimer::timeout,
            this, &CompanionTrackingService::trackingLost);

    connect(socket_, &QUdpSocket::readyRead,
            this, &CompanionTrackingService::receivePendingDatagrams);

    socket_->bind(QHostAddress::LocalHost, kTrackingPort);
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
