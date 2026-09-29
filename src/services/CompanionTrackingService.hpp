// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_SERVICES_COMPANIONTRACKINGSERVICE_HPP
#define DARKSPARK_SERVICES_COMPANIONTRACKINGSERVICE_HPP

#include "models/GazeTarget.hpp"

#include <QObject>

class QProcess;
class QUdpSocket;
class QTimer;

namespace darkspark::services {

class CompanionTrackingService final : public QObject {
    Q_OBJECT

public:
    explicit CompanionTrackingService(QObject* parent = nullptr);
    ~CompanionTrackingService() override;

signals:
    void gazeTargetChanged(models::GazeTarget target);
    void trackingLost();

private:
    void receivePendingDatagrams();
    void startTracker();

    QUdpSocket* socket_;
    QTimer* lostTimer_;
    QProcess* trackerProcess_;
};

}  // namespace darkspark::services

#endif
