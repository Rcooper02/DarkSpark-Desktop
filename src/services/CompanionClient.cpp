// SPDX-License-Identifier: GPL-3.0-or-later
#include "services/CompanionClient.hpp"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

namespace darkspark::services {

namespace {
constexpr int REQUEST_TIMEOUT_MS = 2000;
const QUrl DEFAULT_BASE_URL(QStringLiteral("http://127.0.0.1:8000"));
}  // namespace

CompanionClient::CompanionClient(QObject* parent)
    : CompanionClient(DEFAULT_BASE_URL, parent) {}

CompanionClient::CompanionClient(const QUrl& baseUrl, QObject* parent)
    : QObject(parent), baseUrl_(baseUrl),
      network_(new QNetworkAccessManager(this)) {}

void CompanionClient::setBaseUrl(const QUrl& baseUrl) { baseUrl_ = baseUrl; }

void CompanionClient::press(int page, int row, int column) {
    postControl(QStringLiteral("press"), page, row, column);
}

void CompanionClient::down(int page, int row, int column) {
    postControl(QStringLiteral("down"), page, row, column);
}

void CompanionClient::up(int page, int row, int column) {
    postControl(QStringLiteral("up"), page, row, column);
}

void CompanionClient::setAvailable(bool available) {
    if (availabilityKnown_ && available_ == available) {
        return;
    }
    availabilityKnown_ = true;
    available_ = available;
    emit availabilityChanged(available_);
}

void CompanionClient::checkHealth() {
    if (!baseUrl_.isValid() || baseUrl_.isEmpty()) {
        setAvailable(false);
        return;
    }

    QUrl url = baseUrl_;
    QString basePath = url.path();
    if (basePath.endsWith(QLatin1Char('/'))) {
        basePath.chop(1);
    }
    url.setPath(QStringLiteral("%1/api/connections").arg(basePath));

    QNetworkRequest request(url);
    request.setTransferTimeout(REQUEST_TIMEOUT_MS);
    QNetworkReply* reply = network_->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        const int status =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        setAvailable(reply->error() == QNetworkReply::NoError
                     && status >= 200 && status < 300);
        reply->deleteLater();
    });
}

void CompanionClient::postControl(const QString& action, int page, int row,
                                  int column) {
    if (!baseUrl_.isValid() || baseUrl_.isEmpty()) {
        emit requestFailed(action, page, row, column,
                           QStringLiteral("Companion base URL is invalid"));
        return;
    }
    if (page < 0 || row < 0 || column < 0) {
        emit requestFailed(action, page, row, column,
                           QStringLiteral("Companion location is invalid"));
        return;
    }

    QUrl url = baseUrl_;
    QString basePath = url.path();
    if (basePath.endsWith(QLatin1Char('/'))) {
        basePath.chop(1);
    }
    url.setPath(QStringLiteral("%1/api/location/%2/%3/%4/%5")
                    .arg(basePath)
                    .arg(page)
                    .arg(row)
                    .arg(column)
                    .arg(action));

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));
    request.setTransferTimeout(REQUEST_TIMEOUT_MS);

    QNetworkReply* reply = network_->post(request, QByteArray{});
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, action, page, row, column] {
                const int status =
                    reply->attribute(QNetworkRequest::HttpStatusCodeAttribute)
                        .toInt();
                const bool httpOk = status >= 200 && status < 300;
                const bool networkOk = reply->error() == QNetworkReply::NoError;

                if (networkOk && httpOk) {
                    setAvailable(true);
                    emit requestSucceeded(action, page, row, column);
                } else {
                    setAvailable(false);
                    QString error = reply->errorString();
                    if (status > 0) {
                        error = QStringLiteral("HTTP %1: %2").arg(status).arg(error);
                    }
                    emit requestFailed(action, page, row, column, error);
                }
                reply->deleteLater();
            });
}

}  // namespace darkspark::services
