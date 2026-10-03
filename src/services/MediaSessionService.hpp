// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_SERVICES_MEDIASESSIONSERVICE_HPP
#define DARKSPARK_SERVICES_MEDIASESSIONSERVICE_HPP

#include <QObject>
#include <QString>
#include <QVariantMap>

class QTimer;

namespace darkspark::services {

class MediaSessionService final : public QObject {
    Q_OBJECT

public:
    explicit MediaSessionService(QObject* parent = nullptr);

    [[nodiscard]] QString playerName() const { return playerName_; }
    [[nodiscard]] QString title() const { return title_; }
    [[nodiscard]] QString artist() const { return artist_; }
    [[nodiscard]] QString album() const { return album_; }
    [[nodiscard]] QString artUrl() const { return artUrl_; }
    [[nodiscard]] bool playing() const { return playing_; }

public slots:
    void previous();
    void playPause();
    void next();

signals:
    void mediaChanged(
        const QString& player,
        const QString& title,
        const QString& artist,
        const QString& album,
        const QString& artUrl,
        bool playing);

private:
    void poll();
    void applyProperties(
        const QString& service,
        const QVariantMap& props);
    void clearMediaState();
    QString findPreferredPlayer() const;
    void callPlayerMethod(const QString& method);

    QTimer* timer_;

    QString service_;
    QString playerName_;
    QString title_;
    QString artist_;
    QString album_;
    QString artUrl_;

    bool playing_ = false;
    bool pollInFlight_ = false;
};

}  // namespace darkspark::services

#endif
