// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_SERVICES_COMPANIONCONVERSATIONSERVICE_HPP
#define DARKSPARK_SERVICES_COMPANIONCONVERSATIONSERVICE_HPP

#include <QObject>
#include <QString>
#include <QVector>

class QNetworkAccessManager;

namespace darkspark::services {

/// Sends conversation to native Ollama chat or OpenAI Responses endpoints.
///
/// This service produces text
/// only and has no ability to execute desktop actions.
class CompanionConversationService final : public QObject {
    Q_OBJECT

public:
    explicit CompanionConversationService(QObject* parent = nullptr);
    [[nodiscard]] bool isBusy() const { return requestInFlight_; }

public slots:
    void ask(const QString& transcript);

signals:
    void responseReady(const QString& response);
    void errorOccurred(const QString& message);

private:
    struct Turn {
        QString user;
        QString assistant;
    };

    [[nodiscard]] QString provider() const;
    [[nodiscard]] QString endpoint() const;
    [[nodiscard]] QString model() const;
    [[nodiscard]] QString apiKey() const;

    QNetworkAccessManager* network_;
    QVector<Turn> history_;
    bool requestInFlight_ = false;
};

}  // namespace darkspark::services

#endif  // DARKSPARK_SERVICES_COMPANIONCONVERSATIONSERVICE_HPP
