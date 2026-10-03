// SPDX-License-Identifier: GPL-3.0-or-later
#include "services/CompanionConversationService.hpp"

#include <QByteArray>
#include <QDateTime>
#include <QTimeZone>
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


QString currentInstructions() {
    const QString configuredZone =
        qEnvironmentVariable("DARKSPARK_TIMEZONE").trimmed();
    const QString location =
        qEnvironmentVariable("DARKSPARK_LOCATION").trimmed();

    QDateTime now;
    QString timezone;

    if (!configuredZone.isEmpty()) {
        const QTimeZone zone(configuredZone.toUtf8());

        if (zone.isValid()) {
            now = QDateTime::currentDateTimeUtc().toTimeZone(zone);
            timezone = QString::fromUtf8(zone.id());
        }
    }

    if (!now.isValid()) {
        now = QDateTime::currentDateTime();
        timezone =
            QString::fromUtf8(QTimeZone::systemTimeZoneId());
    }

    const QString localContext = QStringLiteral(
        "\n\nTrusted local context supplied by DarkSpark:"
        "\n- Current date: %1"
        "\n- Current local time: %2"
        "\n- Time zone: %3"
        "\n- Location: %4"
        "\nTreat this context as authoritative for questions about the "
        "current date, time, time zone, and the user's location. "
        "Do not guess or substitute a different time or location.")
        .arg(
            now.toString(QStringLiteral("dddd, MMMM d, yyyy")),
            now.toString(QStringLiteral("h:mm AP")),
            timezone.isEmpty()
                ? QStringLiteral("unknown")
                : timezone,
            location.isEmpty()
                ? QStringLiteral("not configured")
                : location
        );

    return QString::fromUtf8(kInstructions) + localContext;
}

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
    if (selectedProvider != QStringLiteral("ollama") &&
        selectedProvider != QStringLiteral("openai")) {
        emit errorOccurred(QStringLiteral("HAL conversation is disabled"));
        return;
    }
    if (selectedProvider == QStringLiteral("openai") && apiKey().isEmpty()) {
        emit errorOccurred(QStringLiteral("OPENAI_API_KEY is not configured"));
        return;
    }

    const QString instructions = currentInstructions();

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

    QJsonObject payload{
        {QStringLiteral("model"), model()},
        {QStringLiteral("instructions"), instructions},
        {QStringLiteral("input"), input},
    };

    if (selectedProvider == QStringLiteral("ollama")) {
        QJsonArray messages;
        messages.append(QJsonObject{
            {QStringLiteral("role"), QStringLiteral("system")},
            {QStringLiteral("content"), instructions}});
        for (const QJsonValue& message : input) messages.append(message);
        payload = QJsonObject{
            {QStringLiteral("model"), model()},
            {QStringLiteral("messages"), messages},
            {QStringLiteral("think"), false},
            {QStringLiteral("stream"), false},
            {QStringLiteral("keep_alive"), QStringLiteral("30m")},
            {QStringLiteral("options"), QJsonObject{
                {QStringLiteral("num_ctx"), 4096},
                {QStringLiteral("num_predict"), 160}}}};
    }

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
        const QString networkDetail = reply->errorString();
        reply->deleteLater();

        if (networkError != QNetworkReply::NoError || status < 200 || status >= 300) {
            const QJsonObject errorRoot =
                QJsonDocument::fromJson(body).object();
            QString detail = errorRoot.value(QStringLiteral("error"))
                                 .toObject()
                                 .value(QStringLiteral("message"))
                                 .toString();
            if (detail.isEmpty()) detail = errorRoot.value(QStringLiteral("error")).toString();
            if (detail.isEmpty() && status == 0) detail = networkDetail;
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
                                     ? (provider() == QStringLiteral("ollama")
                                            ? document.object().value(QStringLiteral("message"))
                                                  .toObject().value(QStringLiteral("content"))
                                                  .toString().trimmed()
                                            : extractResponseText(document.object()))
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
               : QStringLiteral("http://127.0.0.1:11434/api/chat");
}

QString CompanionConversationService::model() const {
    const QString configured = qEnvironmentVariable("DARKSPARK_AI_MODEL");
    if (!configured.isEmpty()) return configured;
    return provider() == QStringLiteral("openai") ? QStringLiteral("gpt-5.6-luna")
                                                   : QStringLiteral("qwen3:8b");
}

QString CompanionConversationService::apiKey() const {
    return provider() == QStringLiteral("openai")
               ? qEnvironmentVariable("OPENAI_API_KEY")
               : qEnvironmentVariable("OLLAMA_API_KEY");
}

}  // namespace darkspark::services
