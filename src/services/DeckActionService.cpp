// SPDX-License-Identifier: GPL-3.0-or-later
#include "services/DeckActionService.hpp"

#include <QDesktopServices>
#include <QProcess>
#include <QStringList>
#include <QUrl>

namespace darkspark::services {

DeckActionService::DeckActionService(QObject* parent)
    : QObject(parent) {}

void DeckActionService::perform(
    const QString& actionType,
    const QString& target) {

    const QString type = actionType.trimmed();
    const QString value = target.trimmed();

    if (value.isEmpty()) {
        emit actionCompleted(false, QStringLiteral("Action target is empty"));
        return;
    }

    if (type == QStringLiteral("url")) {
        const QUrl url = QUrl::fromUserInput(value);

        if (!url.isValid() || !QDesktopServices::openUrl(url)) {
            emit actionCompleted(false, QStringLiteral("Could not open URL"));
            return;
        }

        emit actionCompleted(true, QStringLiteral("URL opened"));
        return;
    }

    if (type == QStringLiteral("launch_app") ||
        type == QStringLiteral("command")) {

        QStringList parts = QProcess::splitCommand(value);

        if (parts.isEmpty()) {
            emit actionCompleted(false, QStringLiteral("Invalid command"));
            return;
        }

        const QString program = parts.takeFirst();

        if (!QProcess::startDetached(program, parts)) {
            emit actionCompleted(
                false,
                QStringLiteral("Could not start %1").arg(program));
            return;
        }

        emit actionCompleted(
            true,
            QStringLiteral("%1 started").arg(program));
        return;
    }

    if (type == QStringLiteral("page")) {
        emit pageRequested(value);
        emit actionCompleted(
            true,
            QStringLiteral("Page requested: %1").arg(value));
        return;
    }

    emit actionCompleted(
        false,
        QStringLiteral("Unknown deck action: %1").arg(type));
}

}  // namespace darkspark::services
