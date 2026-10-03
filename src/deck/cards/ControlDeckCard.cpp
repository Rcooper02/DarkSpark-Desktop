// SPDX-License-Identifier: GPL-3.0-or-later
#include "deck/cards/ControlDeckCard.hpp"

#include "deck/cards/GlassActionButton.hpp"
#include "deck/config/DeckLayoutStore.hpp"
#include "deck/editor/DeckButtonEditor.hpp"
#include "themes/LegacyTheme.hpp"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayoutItem>
#include <QTimer>
#include <QVBoxLayout>

#include <array>

namespace darkspark::deck::cards {

namespace {
struct ButtonPlan {
    models::ControlAction action;
    const char* label;
};



constexpr std::array<ButtonPlan, 6> kMediaButtons{{
    {models::ControlAction::PreviousTrack, "PREVIOUS"},
    {models::ControlAction::PlayPause, "PLAY / PAUSE"},
    {models::ControlAction::NextTrack, "NEXT"},
    {models::ControlAction::VolumeDown, "VOLUME −"},
    {models::ControlAction::ToggleMute, "MUTE"},
    {models::ControlAction::VolumeUp, "VOLUME +"},
}};

constexpr int kDeckColumns = 10;
constexpr int kDeckRows = 3;

QString mediaIconForAction(models::ControlAction action) {
    switch (action) {
    case models::ControlAction::PreviousTrack:
        return QStringLiteral("media-skip-backward");

    case models::ControlAction::PlayPause:
        return QStringLiteral("media-playback-start");

    case models::ControlAction::NextTrack:
        return QStringLiteral("media-skip-forward");

    case models::ControlAction::VolumeDown:
        return QStringLiteral("audio-volume-low");

    case models::ControlAction::ToggleMute:
        return QStringLiteral("audio-volume-muted");

    case models::ControlAction::VolumeUp:
        return QStringLiteral("audio-volume-high");

    default:
        return {};
    }
}

bool controlActionForTarget(
    const QString& target,
    models::ControlAction& action) {

    if (target == QStringLiteral("launch_firefox"))
        action = models::ControlAction::LaunchFirefox;
    else if (target == QStringLiteral("launch_steam"))
        action = models::ControlAction::LaunchSteam;
    else if (target == QStringLiteral("launch_terminal"))
        action = models::ControlAction::LaunchTerminal;
    else if (target == QStringLiteral("launch_files"))
        action = models::ControlAction::LaunchFiles;
    else if (target == QStringLiteral("launch_system_monitor"))
        action = models::ControlAction::LaunchSystemMonitor;
    else if (target == QStringLiteral("lock_session"))
        action = models::ControlAction::LockSession;
    else
        return false;

    return true;
}
}  // namespace

ControlDeckCard::ControlDeckCard(QWidget* parent)
    : DashboardCard(QStringLiteral("CONTROL DECK"), parent) {
    setSubtitle(QStringLiteral("Allow-listed Fedora launch controls"));
    setAccent(Accent::Cyan);
    setSizeRole(Size::Full);
    setStatusText(QStringLiteral("Ready"));

    auto* content = new QWidget(this);
    auto* split = new QHBoxLayout(content);
    split->setContentsMargins(0, 0, 0, 0);
    split->setSpacing(themes::LegacyTheme::spaceXl());

    keyField_ = new QWidget(content);
    grid_ = new QGridLayout(keyField_);
    grid_->setContentsMargins(0, 0, 0, 0);
    grid_->setHorizontalSpacing(themes::LegacyTheme::spaceMd());
    grid_->setVerticalSpacing(themes::LegacyTheme::spaceLg());

    rebuildDeckGrid();

    split->addWidget(keyField_, 1);

    auto* mediaConsole = new QFrame(content);
    mediaConsole->setObjectName(themes::LegacyTheme::mediaConsoleObjectName());
    mediaConsole->setMinimumWidth(510);
    mediaConsole->setMaximumWidth(620);
    auto* mediaLayout = new QVBoxLayout(mediaConsole);
    mediaLayout->setContentsMargins(themes::LegacyTheme::spaceLg(),
                                    themes::LegacyTheme::spaceLg(),
                                    themes::LegacyTheme::spaceLg(),
                                    themes::LegacyTheme::spaceLg());
    mediaLayout->setSpacing(themes::LegacyTheme::spaceMd());

    auto* mediaTitle = new QLabel(QStringLiteral("MEDIA CONTROL CENTER"), mediaConsole);
    mediaTitle->setProperty("legacyRole", "cardTitle");
    mediaLayout->addWidget(mediaTitle);

    mediaDisplay_ = new QLabel(
        QStringLiteral("NO ACTIVE PLAYER\nREADY FOR MEDIA"), mediaConsole);
    mediaDisplay_->setObjectName(themes::LegacyTheme::mediaDisplayObjectName());
    mediaDisplay_->setAlignment(Qt::AlignCenter);
    mediaDisplay_->setMinimumHeight(96);
    mediaLayout->addWidget(mediaDisplay_);

    auto* mediaGrid = new QGridLayout();
    mediaGrid->setHorizontalSpacing(themes::LegacyTheme::spaceSm());
    mediaGrid->setVerticalSpacing(themes::LegacyTheme::spaceSm());
    for (std::size_t index = 0; index < kMediaButtons.size(); ++index) {
        const ButtonPlan plan = kMediaButtons.at(index);

        auto* button = new GlassActionButton(
            QString(),
            mediaConsole
        );

        // Use standard desktop theme icons rather than text labels.
        button->setDeckIcon(mediaIconForAction(plan.action));

        if (plan.action == models::ControlAction::PlayPause)
            playPauseButton_ = button;

        button->setToolTip(QString::fromUtf8(plan.label));
        button->setAccessibleName(QString::fromUtf8(plan.label));

        // The media console is intentionally denser than the primary key field.
        button->setMinimumSize(104, 72);

        connect(
            button,
            &QPushButton::clicked,
            this,
            [this, action = plan.action]() {
                emit actionRequested(action);
            });
        mediaGrid->addWidget(button, static_cast<int>(index / 3U),
                             static_cast<int>(index % 3U));
    }
    mediaLayout->addLayout(mediaGrid, 1);
    split->addWidget(mediaConsole, 0);
    setContentWidget(content);
}

void ControlDeckCard::rebuildDeckGrid() {
    if (grid_ == nullptr || keyField_ == nullptr)
        return;

    while (QLayoutItem* item = grid_->takeAt(0)) {
        if (QWidget* widget = item->widget())
            widget->deleteLater();

        delete item;
    }

    const QVector<models::DeckButtonConfig> deckButtons =
        config::DeckLayoutStore::load();

    for (int index = 0;
         index < kDeckColumns * kDeckRows;
         ++index) {

        const models::DeckButtonConfig buttonConfig =
            index < deckButtons.size()
                ? deckButtons.at(index)
                : models::DeckButtonConfig{};

        const bool configured =
            buttonConfig.isConfigured();

        auto* button = new GlassActionButton(
            configured && !buttonConfig.title.isEmpty()
                ? buttonConfig.title
                : QStringLiteral("+"),
            keyField_);

        button->setMinimumSize(72, 88);
        button->setMaximumWidth(150);
        button->setDeckSlot(index);
        button->setDeckIcon(buttonConfig.iconPath);

        button->setProperty(
            "legacyRole",
            configured
                ? "deckButton"
                : "emptyDeckSlot");

        connect(
            button,
            &GlassActionButton::longPressed,
            this,
            [this, index]() {
                editSlot(index);
            });

        connect(
            button,
            &GlassActionButton::slotDropped,
            this,
            [this](int fromSlot, int toSlot) {
                moveSlot(fromSlot, toSlot);
            });

        if (configured &&
            buttonConfig.actionType ==
                QStringLiteral("control")) {

            models::ControlAction action =
                models::ControlAction::LaunchFirefox;

            if (controlActionForTarget(
                    buttonConfig.target,
                    action)) {

                connect(
                    button,
                    &QPushButton::clicked,
                    this,
                    [this, action]() {
                        emit actionRequested(action);
                    });
            }
        } else if (configured) {
            const QString actionType = buttonConfig.actionType;
            const QString target = buttonConfig.target;

            connect(
                button,
                &QPushButton::clicked,
                this,
                [this, actionType, target]() {
                    emit customActionRequested(actionType, target);
                });
        }

        grid_->addWidget(
            button,
            index / kDeckColumns,
            index % kDeckColumns);
    }
}

void ControlDeckCard::moveSlot(
    int fromSlot,
    int toSlot) {

    if (fromSlot == toSlot ||
        fromSlot < 0 ||
        toSlot < 0 ||
        fromSlot >= config::DeckLayoutStore::SlotCount ||
        toSlot >= config::DeckLayoutStore::SlotCount) {
        return;
    }

    QVector<models::DeckButtonConfig> buttons =
        config::DeckLayoutStore::load();

    if (fromSlot >= buttons.size() ||
        toSlot >= buttons.size()) {
        return;
    }

    models::DeckButtonConfig source =
        buttons.at(fromSlot);

    models::DeckButtonConfig destination =
        buttons.at(toSlot);

    source.slot = toSlot;
    destination.slot = fromSlot;

    buttons[toSlot] = source;
    buttons[fromSlot] = destination;

    if (!config::DeckLayoutStore::save(buttons)) {
        setState(State::Warning);
        setStatusText(
            QStringLiteral(
                "Could not reorder deck buttons"));
        return;
    }

    setState(State::Normal);
    setStatusText(
        QStringLiteral("Deck button moved"));

    QTimer::singleShot(
        0,
        this,
        [this]() {
            rebuildDeckGrid();
        });
}

void ControlDeckCard::editSlot(int slot) {
    QVector<models::DeckButtonConfig> buttons =
        config::DeckLayoutStore::load();

    if (slot < 0 ||
        slot >= config::DeckLayoutStore::SlotCount)
        return;

    models::DeckButtonConfig current;

    if (slot < buttons.size())
        current = buttons.at(slot);

    current.slot = slot;

    editor::DeckButtonEditor editor(current, this);

    if (editor.exec() != QDialog::Accepted)
        return;

    if (editor.clearRequested()) {
        models::DeckButtonConfig empty;
        empty.slot = slot;
        buttons[slot] = empty;
    } else {
        models::DeckButtonConfig updated =
            editor.config();

        updated.slot = slot;
        buttons[slot] = updated;
    }

    if (!config::DeckLayoutStore::save(buttons)) {
        setState(State::Warning);
        setStatusText(
            QStringLiteral("Could not save deck layout"));
        return;
    }

    setState(State::Normal);
    setStatusText(QStringLiteral("Deck layout saved"));

    // Rebuild after the long-press signal has returned so the
    // button currently emitting the signal is not deleted in-place.
    QTimer::singleShot(
        0,
        this,
        [this]() {
            rebuildDeckGrid();
        });
}

void ControlDeckCard::setMediaState(
    const QString& player,
    const QString& title,
    const QString& artist,
    const QString& album,
    const QString& artUrl,
    bool playing) {

    Q_UNUSED(album);
    Q_UNUSED(artUrl);

    if (mediaDisplay_ != nullptr) {
        if (title.isEmpty()) {
            mediaDisplay_->setText(
                QStringLiteral("NO ACTIVE PLAYER\nREADY FOR MEDIA"));
        } else {
            QString text = title;

            if (!artist.isEmpty())
                text += QStringLiteral("\n") + artist;

            if (!player.isEmpty())
                text += QStringLiteral("\n[%1]").arg(player.toUpper());

            mediaDisplay_->setText(text);
        }
    }

    if (playPauseButton_ != nullptr) {
        playPauseButton_->setDeckIcon(
            playing
                ? QStringLiteral("media-playback-pause")
                : QStringLiteral("media-playback-start"));

        playPauseButton_->setToolTip(
            playing
                ? QStringLiteral("Pause")
                : QStringLiteral("Play"));

        playPauseButton_->setAccessibleName(
            playing
                ? QStringLiteral("Pause")
                : QStringLiteral("Play"));
    }
}

void ControlDeckCard::reportResult(models::ControlAction action, bool success,
                                   const QString& message) {
    setState(success ? State::Normal : State::Warning);
    setStatusText(message);
    if (mediaDisplay_ != nullptr && models::isAudioAction(action)) {
        mediaDisplay_->setText(success ? message.toUpper()
                                       : QStringLiteral("CONTROL ERROR\n%1")
                                             .arg(message.toUpper()));
    }
}

}  // namespace darkspark::deck::cards
