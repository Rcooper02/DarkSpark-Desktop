// SPDX-License-Identifier: GPL-3.0-or-later
#include "deck/config/DeckLayoutStore.hpp"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

namespace darkspark::deck::config {

namespace {

models::DeckButtonConfig makeButton(
    int slot,
    const QString& title,
    const QString& actionType,
    const QString& target) {

    models::DeckButtonConfig button;
    button.slot = slot;
    button.title = title;
    button.actionType = actionType;
    button.target = target;
    return button;
}

}  // namespace

QString DeckLayoutStore::layoutPath() {
    QString root =
        QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);

    if (root.isEmpty()) {
        root = QDir::home().filePath(QStringLiteral(".config/DarkSpark"));
    }

    QDir().mkpath(root);
    return QDir(root).filePath(QStringLiteral("deck-layout.json"));
}

QVector<models::DeckButtonConfig> DeckLayoutStore::defaults() {
    QVector<models::DeckButtonConfig> buttons;
    buttons.reserve(SlotCount);

    for (int slot = 0; slot < SlotCount; ++slot) {
        models::DeckButtonConfig button;
        button.slot = slot;
        buttons.append(button);
    }

    buttons[0] = makeButton(
        0, QStringLiteral("FIREFOX"),
        QStringLiteral("control"),
        QStringLiteral("launch_firefox"));

    buttons[1] = makeButton(
        1, QStringLiteral("STEAM"),
        QStringLiteral("control"),
        QStringLiteral("launch_steam"));

    buttons[2] = makeButton(
        2, QStringLiteral("TERMINAL"),
        QStringLiteral("control"),
        QStringLiteral("launch_terminal"));

    buttons[3] = makeButton(
        3, QStringLiteral("FILES"),
        QStringLiteral("control"),
        QStringLiteral("launch_files"));

    buttons[4] = makeButton(
        4, QStringLiteral("SYSTEM MONITOR"),
        QStringLiteral("control"),
        QStringLiteral("launch_system_monitor"));

    buttons[5] = makeButton(
        5, QStringLiteral("LOCK"),
        QStringLiteral("control"),
        QStringLiteral("lock_session"));

    return buttons;
}

QVector<models::DeckButtonConfig> DeckLayoutStore::load() {
    const QString path = layoutPath();
    QFile file(path);

    if (!file.exists()) {
        const QVector<models::DeckButtonConfig> initial = defaults();
        save(initial);
        return initial;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        return defaults();
    }

    QJsonParseError error;
    const QJsonDocument document =
        QJsonDocument::fromJson(file.readAll(), &error);

    if (error.error != QJsonParseError::NoError ||
        !document.isObject()) {
        return defaults();
    }

    QVector<models::DeckButtonConfig> result = defaults();

    const QJsonArray buttons =
        document.object().value(QStringLiteral("buttons")).toArray();

    for (const QJsonValue& value : buttons) {
        if (!value.isObject()) continue;

        const QJsonObject object = value.toObject();
        const int slot =
            object.value(QStringLiteral("slot")).toInt(-1);

        if (slot < 0 || slot >= SlotCount) continue;

        models::DeckButtonConfig button;
        button.slot = slot;
        button.title =
            object.value(QStringLiteral("title")).toString();
        button.iconPath =
            object.value(QStringLiteral("icon")).toString();
        button.actionType =
            object.value(QStringLiteral("actionType")).toString();
        button.target =
            object.value(QStringLiteral("target")).toString();

        result[slot] = button;
    }

    return result;
}

bool DeckLayoutStore::save(
    const QVector<models::DeckButtonConfig>& buttons) {

    QJsonArray array;

    for (const models::DeckButtonConfig& button : buttons) {
        QJsonObject object;
        object.insert(QStringLiteral("slot"), button.slot);
        object.insert(QStringLiteral("title"), button.title);
        object.insert(QStringLiteral("icon"), button.iconPath);
        object.insert(QStringLiteral("actionType"), button.actionType);
        object.insert(QStringLiteral("target"), button.target);
        array.append(object);
    }

    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("buttons"), array);

    QSaveFile file(layoutPath());
    if (!file.open(QIODevice::WriteOnly)) return false;

    file.write(
        QJsonDocument(root).toJson(QJsonDocument::Indented));

    return file.commit();
}

}  // namespace darkspark::deck::config
