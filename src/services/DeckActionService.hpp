// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_SERVICES_DECKACTIONSERVICE_HPP
#define DARKSPARK_SERVICES_DECKACTIONSERVICE_HPP

#include <QObject>
#include <QString>

namespace darkspark::services {

class DeckActionService final : public QObject {
    Q_OBJECT

public:
    explicit DeckActionService(QObject* parent = nullptr);

public slots:
    void perform(const QString& actionType, const QString& target);

signals:
    void actionCompleted(bool success, const QString& message);
    void pageRequested(const QString& pageName);
};

}  // namespace darkspark::services

#endif
