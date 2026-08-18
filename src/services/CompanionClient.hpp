// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_SERVICES_COMPANIONCLIENT_HPP
#define DARKSPARK_SERVICES_COMPANIONCLIENT_HPP

#include <QObject>
#include <QUrl>

class QNetworkAccessManager;

namespace darkspark::services {

/// Minimal asynchronous client for Bitfocus Companion's documented HTTP
/// location-control API. Companion remains a separate process; DarkSpark only
/// sends control gestures and never blocks the GUI thread waiting for a reply.
class CompanionClient : public QObject {
    Q_OBJECT

public:
    explicit CompanionClient(QObject* parent = nullptr);
    explicit CompanionClient(const QUrl& baseUrl, QObject* parent = nullptr);

    [[nodiscard]] QUrl baseUrl() const { return baseUrl_; }
    void setBaseUrl(const QUrl& baseUrl);

    void press(int page, int row, int column);
    void down(int page, int row, int column);
    void up(int page, int row, int column);
    void checkHealth();

    [[nodiscard]] bool availabilityKnown() const { return availabilityKnown_; }
    [[nodiscard]] bool available() const { return available_; }

signals:
    void requestSucceeded(const QString& action, int page, int row, int column);
    void requestFailed(const QString& action, int page, int row, int column,
                       const QString& errorMessage);
    void availabilityChanged(bool available);

private:
    void setAvailable(bool available);
    void postControl(const QString& action, int page, int row, int column);

    QUrl baseUrl_;
    QNetworkAccessManager* network_ = nullptr;
    bool availabilityKnown_ = false;
    bool available_ = false;
};

}  // namespace darkspark::services

#endif  // DARKSPARK_SERVICES_COMPANIONCLIENT_HPP
