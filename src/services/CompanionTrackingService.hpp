// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_SERVICES_COMPANIONTRACKINGSERVICE_HPP
#define DARKSPARK_SERVICES_COMPANIONTRACKINGSERVICE_HPP

#include "models/GazeTarget.hpp"

#include <QObject>

class QUdpSocket;
class QTimer;

namespace darkspark::services {

class CompanionTrackingService final : public QObject {
    Q_OBJECT

public:
    explicit CompanionTrackingService(QObject* parent = nullptr);

signals:
    void gazeTargetChanged(models::GazeTarget target);
    void trackingLost();

private:
    void receivePendingDatagrams();

    QUdpSocket* socket_;
    QTimer* lostTimer_;
};

}  // namespace darkspark::services

#endif
