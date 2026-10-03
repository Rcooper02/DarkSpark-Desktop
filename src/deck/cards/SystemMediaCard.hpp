// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_DECK_CARDS_SYSTEMMEDIACARD_HPP
#define DARKSPARK_DECK_CARDS_SYSTEMMEDIACARD_HPP

#include "deck/cards/DashboardCard.hpp"
#include "models/MetricSample.hpp"

class QLabel;
class QProgressBar;

namespace darkspark::deck::cards {

class FeishinCard;

/// Full-width System page surface:
/// left half = system monitoring
/// right half = embedded Feishin.
class SystemMediaCard final : public DashboardCard {
    Q_OBJECT

public:
    explicit SystemMediaCard(QWidget* parent = nullptr);

    void receiveTelemetry(const models::MetricSample& sample);

private:
    QLabel* cpuValue_ = nullptr;
    QLabel* memoryValue_ = nullptr;
    QLabel* gpuValue_ = nullptr;
    QLabel* networkValue_ = nullptr;
    QLabel* storageValue_ = nullptr;

    QProgressBar* cpuBar_ = nullptr;
    QProgressBar* memoryBar_ = nullptr;

    FeishinCard* feishin_ = nullptr;
};

}  // namespace darkspark::deck::cards

#endif
