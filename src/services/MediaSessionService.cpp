// SPDX-License-Identifier: GPL-3.0-or-later
#include "services/MediaSessionService.hpp"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusReply>
#include <QTimer>
#include <QVariantMap>

namespace darkspark::services {

namespace {

constexpr auto kPath = "/org/mpris/MediaPlayer2";
constexpr auto kPlayerInterface = "org.mpris.MediaPlayer2.Player";
constexpr auto kPropertiesInterface = "org.freedesktop.DBus.Properties";

QString metadataString(
    const QVariantMap& metadata,
    const QString& key) {

    return metadata.value(key).toString();
}

QString metadataArtist(
    const QVariantMap& metadata) {

    const QVariant value =
        metadata.value(QStringLiteral("xesam:artist"));

    const QStringList artists =
        value.toStringList();

    return artists.join(QStringLiteral(", "));
}

}  // namespace

MediaSessionService::MediaSessionService(QObject* parent)
    : QObject(parent),
      timer_(new QTimer(this)) {

    timer_->setInterval(2000);

    connect(
        timer_,
        &QTimer::timeout,
        this,
        &MediaSessionService::poll);

    timer_->start();

    poll();
}

QString MediaSessionService::findPreferredPlayer() const {
    QDBusConnectionInterface* bus =
        QDBusConnection::sessionBus().interface();

    if (bus == nullptr)
        return {};

    const QDBusReply<QStringList> reply =
        bus->registeredServiceNames();

    if (!reply.isValid())
        return {};

    QString fallback;

    for (const QString& name : reply.value()) {
        if (!name.startsWith(
                QStringLiteral(
                    "org.mpris.MediaPlayer2."))) {
            continue;
        }

        if (name == QStringLiteral(
                "org.mpris.MediaPlayer2.playerctld")) {
            continue;
        }

        if (name.contains(
                QStringLiteral("Feishin"),
                Qt::CaseInsensitive)) {
            return name;
        }

        if (fallback.isEmpty())
            fallback = name;
    }

    return fallback;
}

void MediaSessionService::clearMediaState() {
    const bool hadState =
        !service_.isEmpty() ||
        !playerName_.isEmpty() ||
        !title_.isEmpty() ||
        !artist_.isEmpty() ||
        !album_.isEmpty() ||
        !artUrl_.isEmpty() ||
        playing_;

    service_.clear();
    playerName_.clear();
    title_.clear();
    artist_.clear();
    album_.clear();
    artUrl_.clear();
    playing_ = false;

    if (hadState) {
        emit mediaChanged(
            {},
            {},
            {},
            {},
            {},
            false);
    }
}

void MediaSessionService::poll() {
    if (pollInFlight_)
        return;

    QString discovered = service_;

    // Avoid enumerating every D-Bus service on every 500 ms tick once
    // a usable player has already been found.
    if (discovered.isEmpty()) {
        discovered = findPreferredPlayer();
    }

    if (discovered.isEmpty()) {
        clearMediaState();
        return;
    }

    QDBusInterface properties(
        discovered,
        QString::fromUtf8(kPath),
        QString::fromUtf8(kPropertiesInterface),
        QDBusConnection::sessionBus());

    if (!properties.isValid()) {
        clearMediaState();
        return;
    }

    pollInFlight_ = true;

    const QDBusPendingCall call =
        properties.asyncCall(
            QStringLiteral("GetAll"),
            QString::fromUtf8(kPlayerInterface));

    auto* watcher =
        new QDBusPendingCallWatcher(
            call,
            this);

    connect(
        watcher,
        &QDBusPendingCallWatcher::finished,
        this,
        [this, discovered](
            QDBusPendingCallWatcher* finished) {

            pollInFlight_ = false;

            const QDBusPendingReply<QVariantMap> reply =
                *finished;

            finished->deleteLater();

            if (reply.isError()) {
                // The player may have disappeared or restarted.
                // Clear the cached service so the next poll can discover
                // whichever MPRIS player is currently available.
                if (service_ == discovered) {
                    clearMediaState();
                }
                return;
            }

            service_ = discovered;
            applyProperties(
                discovered,
                reply.value());
        });
}

void MediaSessionService::applyProperties(
    const QString& service,
    const QVariantMap& props) {

    const QString status =
        props.value(
            QStringLiteral("PlaybackStatus"))
            .toString();

    QVariantMap metadata;

    const QVariant metadataValue =
        props.value(
            QStringLiteral("Metadata"));

    if (metadataValue.canConvert<QVariantMap>()) {
        metadata =
            metadataValue.toMap();
    } else if (
        metadataValue.canConvert<QDBusArgument>()) {

        const QDBusArgument argument =
            metadataValue.value<QDBusArgument>();

        metadata =
            qdbus_cast<QVariantMap>(argument);
    }

    const QString newTitle =
        metadataString(
            metadata,
            QStringLiteral("xesam:title"));

    const QString newArtist =
        metadataArtist(metadata);

    const QString newAlbum =
        metadataString(
            metadata,
            QStringLiteral("xesam:album"));

    const QString newArt =
        metadataString(
            metadata,
            QStringLiteral("mpris:artUrl"));

    const bool newPlaying =
        status == QStringLiteral("Playing");

    const QString newPlayer =
        service.section(
            QLatin1Char('.'),
            -1);

    if (newPlayer == playerName_ &&
        newTitle == title_ &&
        newArtist == artist_ &&
        newAlbum == album_ &&
        newArt == artUrl_ &&
        newPlaying == playing_) {
        return;
    }

    playerName_ = newPlayer;
    title_ = newTitle;
    artist_ = newArtist;
    album_ = newAlbum;
    artUrl_ = newArt;
    playing_ = newPlaying;

    emit mediaChanged(
        playerName_,
        title_,
        artist_,
        album_,
        artUrl_,
        playing_);
}

void MediaSessionService::callPlayerMethod(
    const QString& method) {

    if (service_.isEmpty())
        service_ = findPreferredPlayer();

    if (service_.isEmpty())
        return;

    QDBusInterface player(
        service_,
        QString::fromUtf8(kPath),
        QString::fromUtf8(kPlayerInterface),
        QDBusConnection::sessionBus());

    if (!player.isValid())
        return;

    player.asyncCall(method);

    QTimer::singleShot(
        100,
        this,
        &MediaSessionService::poll);
}

void MediaSessionService::previous() {
    callPlayerMethod(
        QStringLiteral("Previous"));
}

void MediaSessionService::playPause() {
    callPlayerMethod(
        QStringLiteral("PlayPause"));
}

void MediaSessionService::next() {
    callPlayerMethod(
        QStringLiteral("Next"));
}

}  // namespace darkspark::services
