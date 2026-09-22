// SPDX-License-Identifier: GPL-3.0-or-later
#include "deck/companion/CompanionCard.hpp"

#include "deck/companion/CompanionFaceWidget.hpp"
#include "themes/LegacyTheme.hpp"

#include <QDateTime>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

#include <array>

namespace darkspark::deck::companion {

using models::CompanionState;
using themes::LegacyTheme;

namespace {

struct StateControl {
    CompanionState state;
    const char* label;
};

constexpr std::array<StateControl, 6> kControls{{
    {CompanionState::Dormant, "SLEEP"},
    {CompanionState::Idle, "IDLE"},
    {CompanionState::Listening, "LISTEN"},
    {CompanionState::Thinking, "THINK"},
    {CompanionState::Speaking, "SPEAK"},
    {CompanionState::Alert, "ALERT"},
}};

QLabel* makeLabel(const QString& text, const char* role, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    label->setProperty("legacyRole", role);
    return label;
}

QWidget* makeStatusRow(const QString& name,
                       const QString& state,
                       QWidget* parent) {
    auto* row = new QWidget(parent);
    auto* layout = new QHBoxLayout(row);

    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(LegacyTheme::spaceSm());

    auto* dot = makeLabel(QStringLiteral("●"), "statusText", row);
    dot->setProperty("legacyState", "error");
    dot->setFixedWidth(18);

    auto* nameLabel = makeLabel(name, "cardTitle", row);
    auto* stateLabel = makeLabel(state, "statusText", row);

    layout->addWidget(dot);
    layout->addWidget(nameLabel);
    layout->addStretch(1);
    layout->addWidget(stateLabel);

    return row;
}

QString greetingForHour(int hour) {
    if (hour < 12) {
        return QStringLiteral("GOOD MORNING, STARBADGER.");
    }
    if (hour < 18) {
        return QStringLiteral("GOOD AFTERNOON, STARBADGER.");
    }
    return QStringLiteral("GOOD EVENING, STARBADGER.");
}

}  // namespace

CompanionCard::CompanionCard(QWidget* parent)
    : DashboardCard(QStringLiteral("DARKSPARK // HAL"), parent),
      face_(new CompanionFaceWidget(this)),
      stateLabel_(new QLabel(this)) {

    setSubtitle(QStringLiteral("OPTICAL INTELLIGENCE CORE"));
    setAccent(Accent::None);

    // HAL owns Page 1. Use the complete XENEON width.
    setSizeRole(Size::Full);
    // Keep the complete card, footer, and page indicator inside the XENEON's
    // 720-pixel height. The internal layout can still expand on taller screens.
    setMinimumHeight(440);

    setStatusText(
        QStringLiteral("VISION ONLINE // PIXY LINK READY // HAL WATCHING"));

    auto* content = new QWidget(this);
    auto* root = new QVBoxLayout(content);

    root->setContentsMargins(
        LegacyTheme::spaceLg(),
        LegacyTheme::spaceSm(),
        LegacyTheme::spaceLg(),
        LegacyTheme::spaceSm());

    root->setSpacing(LegacyTheme::spaceSm());

    // -----------------------------------------------------------------
    // MAIN THREE-COLUMN CONSOLE
    // -----------------------------------------------------------------

    auto* console = new QHBoxLayout();
    console->setSpacing(LegacyTheme::spaceXl());

    // -------------------------------------------------------------
    // LEFT: identity + greeting
    // -------------------------------------------------------------

    auto* leftPanel = new QWidget(content);
    leftPanel->setMinimumWidth(470);
    leftPanel->setMaximumWidth(560);

    auto* left = new QVBoxLayout(leftPanel);
    left->setContentsMargins(0, 0, 0, 0);
    left->setSpacing(LegacyTheme::spaceSm());

    auto* brand = makeLabel(
        QStringLiteral("DARKSPARK"),
        "applicationTitle",
        leftPanel);

    auto* systemLine = makeLabel(
        QStringLiteral("SYSTEMS ONLINE // ALWAYS WITH YOU"),
        "pageSubtitle",
        leftPanel);

    auto* greeting = makeLabel(
        greetingForHour(QDateTime::currentDateTime().time().hour()),
        "pageTitle",
        leftPanel);

    greeting->setWordWrap(true);

    auto* assistantLine = makeLabel(
        QStringLiteral("I'M HERE.\nHOW CAN I HELP?"),
        "cardTitle",
        leftPanel);

    assistantLine->setWordWrap(true);

    left->addWidget(brand);
    left->addWidget(systemLine);
    left->addSpacing(LegacyTheme::space2xl());
    left->addWidget(greeting);
    left->addSpacing(LegacyTheme::spaceLg());
    left->addWidget(assistantLine);
    left->addStretch(1);

    auto* watcher = makeLabel(
        QStringLiteral("●  HAL // WATCHING"),
        "companionState",
        leftPanel);

    left->addWidget(watcher);

    // -------------------------------------------------------------
    // CENTER: the live tracking HAL eye
    // -------------------------------------------------------------

    auto* centerPanel = new QWidget(content);
    auto* center = new QVBoxLayout(centerPanel);

    center->setContentsMargins(0, 0, 0, 0);
    center->setSpacing(LegacyTheme::spaceXs());

    // Preserve a substantial optical core without forcing the surrounding
    // controls and status footer below the 720-pixel display boundary.
    face_->setMinimumSize(460, 300);

    center->addWidget(face_, 1, Qt::AlignCenter);

    stateLabel_->setProperty("legacyRole", "companionState");
    stateLabel_->setAlignment(Qt::AlignCenter);

    center->addWidget(stateLabel_);

    // -------------------------------------------------------------
    // RIGHT: time + HAL state display
    // -------------------------------------------------------------

    auto* rightPanel = new QWidget(content);
    rightPanel->setMinimumWidth(430);
    rightPanel->setMaximumWidth(520);

    auto* right = new QVBoxLayout(rightPanel);
    right->setContentsMargins(0, 0, 0, 0);
    right->setSpacing(LegacyTheme::spaceSm());

    auto* clock = makeLabel(QString(), "primaryValue", rightPanel);
    clock->setAlignment(Qt::AlignCenter);

    auto* date = makeLabel(QString(), "pageSubtitle", rightPanel);
    date->setAlignment(Qt::AlignCenter);

    right->addWidget(clock);
    right->addWidget(date);
    right->addSpacing(LegacyTheme::spaceMd());

    right->addWidget(
        makeStatusRow(
            QStringLiteral("VISION"),
            QStringLiteral("ONLINE"),
            rightPanel));

    right->addWidget(
        makeStatusRow(
            QStringLiteral("LISTENING"),
            QStringLiteral("STANDBY"),
            rightPanel));

    right->addWidget(
        makeStatusRow(
            QStringLiteral("THINKING"),
            QStringLiteral("READY"),
            rightPanel));

    right->addWidget(
        makeStatusRow(
            QStringLiteral("SPEAKING"),
            QStringLiteral("READY"),
            rightPanel));

    right->addStretch(1);

    auto* motto = makeLabel(
        QStringLiteral("A MORE INTELLIGENT TOMORROW."),
        "pageSubtitle",
        rightPanel);

    motto->setAlignment(Qt::AlignCenter);
    right->addWidget(motto);

    console->addWidget(leftPanel, 0);
    console->addWidget(centerPanel, 1);
    console->addWidget(rightPanel, 0);

    root->addLayout(console, 1);

    // -----------------------------------------------------------------
    // BOTTOM: HAL development state controls
    // -----------------------------------------------------------------

    auto* controls = new QGridLayout();
    controls->setHorizontalSpacing(LegacyTheme::spaceSm());
    controls->setVerticalSpacing(LegacyTheme::spaceXs());

    for (std::size_t index = 0; index < kControls.size(); ++index) {
        const StateControl control = kControls.at(index);

        auto* button = new QPushButton(
            QString::fromUtf8(control.label),
            content);

        button->setMinimumHeight(LegacyTheme::touchTargetMin());

        connect(
            button,
            &QPushButton::clicked,
            this,
            [this, state = control.state]() {
                emit stateRequested(state);
            });

        controls->addWidget(
            button,
            0,
            static_cast<int>(index));
    }

    root->addLayout(controls);

    // -----------------------------------------------------------------
    // Clock refresh
    // -----------------------------------------------------------------

    auto* clockTimer = new QTimer(this);

    const auto refreshClock = [clock, date, greeting]() {
        const QDateTime now = QDateTime::currentDateTime();

        clock->setText(
            now.time().toString(QStringLiteral("h:mm AP")));

        date->setText(
            now.date().toString(QStringLiteral("ddd MMM d, yyyy"))
                .toUpper());

        greeting->setText(
            greetingForHour(now.time().hour()));
    };

    connect(
        clockTimer,
        &QTimer::timeout,
        this,
        refreshClock);

    clockTimer->setInterval(1000);
    clockTimer->start();

    refreshClock();

    setContentWidget(content);
    setCompanionState(CompanionState::Idle);
}

void CompanionCard::setCompanionState(CompanionState state) {
    face_->setCompanionState(state);

    const std::string_view name =
        models::companionStateName(state);

    stateLabel_->setText(
        QStringLiteral("HAL // %1")
            .arg(
                QString::fromUtf8(
                    name.data(),
                    static_cast<qsizetype>(name.size()))
                    .toUpper()));

    stateLabel_->setProperty(
        "companionState",
        static_cast<int>(state));

    stateLabel_->style()->unpolish(stateLabel_);
    stateLabel_->style()->polish(stateLabel_);
}

CompanionState CompanionCard::companionState() const {
    return face_->companionState();
}

void CompanionCard::setGazeTarget(models::GazeTarget target) {
    face_->setGazeTarget(target);

    setStatusText(
        QStringLiteral(
            "VISION ONLINE // SUBJECT LOCK // HAL WATCHING"));
}

void CompanionCard::clearGazeTarget() {
    face_->clearGazeTarget();

    setStatusText(
        QStringLiteral(
            "VISION ONLINE // SEARCHING // HAL WATCHING"));
}

}  // namespace darkspark::deck::companion
