// SPDX-License-Identifier: GPL-3.0-or-later
#include "deck/cards/FeishinCard.hpp"

#include <QDir>
#include <QStandardPaths>
#include <QUrl>
#include <QVBoxLayout>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineSettings>
#include <QWebEngineView>

namespace darkspark::deck::cards {

FeishinCard::FeishinCard(QWidget* parent)
    : DashboardCard(QStringLiteral("FEISHIN"), parent) {

    setSubtitle(QStringLiteral("Music"));
    setAccent(Accent::Purple);
    setSizeRole(Size::Full);
    setStatusText(QStringLiteral("Local web client"));

    auto* content = new QWidget(this);
    auto* layout = new QVBoxLayout(content);

    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    //
    // Persistent Feishin browser profile.
    //
    // This preserves Feishin's localStorage, IndexedDB and cookies across
    // DarkSpark restarts so the user does not need to add the server again.
    //
    const QString appData =
        QStandardPaths::writableLocation(
            QStandardPaths::AppDataLocation);

    const QString storagePath =
        QDir(appData).filePath(
            QStringLiteral("webengine/feishin/storage"));

    const QString cachePath =
        QDir(appData).filePath(
            QStringLiteral("webengine/feishin/cache"));

    QDir().mkpath(storagePath);
    QDir().mkpath(cachePath);

    profile_ =
        new QWebEngineProfile(
            QStringLiteral("DarkSpark-Feishin"),
            this);

    profile_->setPersistentStoragePath(storagePath);
    profile_->setCachePath(cachePath);

    profile_->setHttpCacheType(
        QWebEngineProfile::DiskHttpCache);

    profile_->setPersistentCookiesPolicy(
        QWebEngineProfile::ForcePersistentCookies);

    auto* page =
        new QWebEnginePage(profile_, this);

    // Keep Feishin alive while its deck page is hidden. Chromium normally
    // throttles background/occluded pages, which causes a visible pause when
    // returning to the System page.
    page->setLifecycleState(
        QWebEnginePage::LifecycleState::Active);

    connect(
        page,
        &QWebEnginePage::recommendedStateChanged,
        page,
        [page](QWebEnginePage::LifecycleState state) {
            if (state != QWebEnginePage::LifecycleState::Active) {
                page->setLifecycleState(
                    QWebEnginePage::LifecycleState::Active);
            }
        });

    view_ =
        new QWebEngineView(content);

    view_->setPage(page);

    view_->settings()->setAttribute(
        QWebEngineSettings::PlaybackRequiresUserGesture,
        false);

    view_->setUrl(
        QUrl(QStringLiteral(
            "http://127.0.0.1:9180")));

    layout->addWidget(view_, 1);

    setContentWidget(content);
}


}  // namespace darkspark::deck::cards
