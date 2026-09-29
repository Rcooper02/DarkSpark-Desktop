// SPDX-License-Identifier: GPL-3.0-or-later
#include "services/CompanionConversationService.hpp"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

namespace darkspark::services {

namespace {

Q_LOGGING_CATEGORY(lcConversation, "darkspark.companion.conversation")

constexpr auto kInstructions =
    "You are HAL, the calm onboard intelligence in DarkSpark Desktop. "
    "Speak in a measured, precise, quietly unsettling manner, while remaining "
    "helpful and safe. Keep replies concise: normally one or two short "
    "sentences suitable for text-to-speech. The user's spoken name is Star "
    "Badger. Use it sparingly. Never claim that you operated the computer; "
    "desktop controls are handled by a separate local allow-listed system.";

QString extractResponseText(const QJsonObject& root) {
    const QJsonArray output = root.value(QStringLiteral("output")).toArray();
    for (const QJsonValue& itemValue : output) {
        const QJsonArray content =
            itemValue.toObject().value(QStringLiteral("content")).toArray();
        for (const QJsonValue& contentValue : content) {
            const QJsonObject part = contentValue.toObject();
            if (part.value(QStringLiteral("type")).toString() ==
                QStringLiteral("output_text")) {
                return part.value(QStringLiteral("text")).toString().trimmed();
            }
        }
    }
    return root.value(QStringLiteral("output_text")).toString().trimmed();
}

}  // namespace

CompanionConversationService::CompanionConversationService(QObject* parent)
    : QObject(parent), network_(new QNetworkAccessManager(this)) {}

void CompanionConversationService::ask(const QString& transcript) {
    if (requestInFlight_ || transcript.trimmed().isEmpty()) return;

    const QString selectedProvider = provider();
    if (selectedProvider == QStringLiteral("none")) {
        emit errorOccurred(QStringLiteral("HAL conversation is disabled"));
        return;
    }
    if (selectedProvider == QStringLiteral("openai") && apiKey().isEmpty()) {
        emit errorOccurred(QStringLiteral("OPENAI_API_KEY is not configured"));
        return;
    }

    QJsonArray input;
    for (const Turn& turn : history_) {
        input.append(QJsonObject{{QStringLiteral("role"), QStringLiteral("user")},
                                 {QStringLiteral("content"), turn.user}});
        input.append(QJsonObject{
            {QStringLiteral("role"), QStringLiteral("assistant")},
            {QStringLiteral("content"), turn.assistant}});
    }
    input.append(QJsonObject{{QStringLiteral("role"), QStringLiteral("user")},
                             {QStringLiteral("content"), transcript}});

    const QJsonObject payload{
        {QStringLiteral("model"), model()},
        {QStringLiteral("instructions"), QString::fromUtf8(kInstructions)},
        {QStringLiteral("input"), input},
    };

    QNetworkRequest request{QUrl(endpoint())};
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));
    request.setTransferTimeout(60000);
    const QString key = apiKey();
    if (!key.isEmpty()) {
        request.setRawHeader("Authorization",
                             QByteArrayLiteral("Bearer ") + key.toUtf8());
    }

    requestInFlight_ = true;
    qCInfo(lcConversation) << "HAL conversation request via" << selectedProvider
                           << "using" << model();
    QNetworkReply* reply =
        network_->post(request, QJsonDocument(payload).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply, transcript]() {
        requestInFlight_ = false;
        const QByteArray body = reply->readAll();
        const int status =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QNetworkReply::NetworkError networkError = reply->error();
        reply->deleteLater();

        if (networkError != QNetworkReply::NoError || status < 200 || status >= 300) {
            const QJsonObject errorRoot =
                QJsonDocument::fromJson(body).object();
            QString detail = errorRoot.value(QStringLiteral("error"))
                                 .toObject()
                                 .value(QStringLiteral("message"))
                                 .toString();
            if (detail.isEmpty()) {
                detail = QStringLiteral("Conversation request failed (HTTP %1)")
                             .arg(status);
            }
            qCWarning(lcConversation) << detail;
            emit errorOccurred(detail.left(240));
            return;
        }

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
        const QString response = document.isObject()
                                     ? extractResponseText(document.object())
                                     : QString();
        if (parseError.error != QJsonParseError::NoError || response.isEmpty()) {
            qCWarning(lcConversation) << "HAL received an invalid AI response";
            emit errorOccurred(QStringLiteral("HAL received an invalid AI response"));
            return;
        }

        history_.append(Turn{transcript, response});
        while (history_.size() > 4) history_.removeFirst();
        emit responseReady(response);
    });
}

QString CompanionConversationService::provider() const {
    const QString value = qEnvironmentVariable("DARKSPARK_AI_PROVIDER").toLower();
    return value.isEmpty() ? QStringLiteral("ollama") : value;
}

QString CompanionConversationService::endpoint() const {
    const QString configured = qEnvironmentVariable("DARKSPARK_AI_ENDPOINT");
    if (!configured.isEmpty()) return configured;
    return provider() == QStringLiteral("openai")
               ? QStringLiteral("https://api.openai.com/v1/responses")
               : QStringLiteral("http://127.0.0.1:11434/v1/responses");
}

QString CompanionConversationService::model() const {
    const QString configured = qEnvironmentVariable("DARKSPARK_AI_MODEL");
    if (!configured.isEmpty()) return configured;
    return provider() == QStringLiteral("openai") ? QStringLiteral("gpt-5.6-luna")
                                                   : QStringLiteral("qwen3:4b");
}

QString CompanionConversationService::apiKey() const {
    return provider() == QStringLiteral("openai")
               ? qEnvironmentVariable("OPENAI_API_KEY")
               : qEnvironmentVariable("OLLAMA_API_KEY");
}

}  // namespace darkspark::services
