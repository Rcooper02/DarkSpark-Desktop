// SPDX-License-Identifier: GPL-3.0-or-later
#include "application/Application.hpp"

#include "deck/DeckWindow.hpp"
#include "desktop/DesktopWindow.hpp"
#include "interfaces/ITelemetryProvider.hpp"
#include "models/MetricSample.hpp"
#include "services/CpuTelemetryService.hpp"
#include "services/MemoryTelemetryService.hpp"
#include "services/DesktopControlService.hpp"
#include "services/DeckActionService.hpp"
#include "services/MediaSessionService.hpp"
#include "services/CompanionTrackingService.hpp"
#include "services/CompanionSpeechService.hpp"
#include "services/CompanionListeningService.hpp"
#include "services/CompanionConversationService.hpp"
#include <QRegularExpression>
#include "models/CompanionState.hpp"
#include "themes/LegacyTheme.hpp"

#include <QApplication>
#include <QGuiApplication>
#include <QList>
#include <QLoggingCategory>
#include <QScreen>
#include <QStringList>
#include <QTime>

namespace darkspark::application {

namespace {
Q_LOGGING_CATEGORY(lcApp, "darkspark.application")
}

Application::Application(QApplication& qtApp) : qtApp_(qtApp) {}

Application::~Application() = default;

LaunchOptions Application::parseArguments(const QStringList& arguments) {
    LaunchOptions options;

    for (int i = 1; i < arguments.size(); ++i) {
        const QString& arg = arguments.at(i);
        if (arg == QStringLiteral("--deck")) {
            options.mode = StartupMode::Deck;
        } else if (arg == QStringLiteral("--deck-screen")) {
            options.mode = StartupMode::Deck;
            if (i + 1 < arguments.size()) {
                bool ok = false;
                const int index = arguments.at(i + 1).toInt(&ok);
                // Store whatever was requested (including invalid) and let run()
                // validate against the actual screen list with a safe fallback.
                options.deckScreenIndex = ok ? index : -2;  // -2 marks "invalid"
                ++i;  // consume the value
            } else {
                qCWarning(lcApp) << "--deck-screen given without an index; "
                                    "using primary display";
                options.deckScreenIndex = -1;
            }
        } else {
            qCWarning(lcApp) << "Ignoring unrecognized argument:" << arg;
        }
    }

    return options;
}

int Application::run(const LaunchOptions& options) {
    themes::LegacyTheme::apply(&qtApp_);

    // Telemetry is created and started once, before mode selection, and runs
    // for the Application lifetime. A Deck window may be created later (or
    // never), so sampling is not tied to any window's existence.
    startTelemetry();
    startDesktopControls();
    startDeckActions();
    startMediaSession();
    startCompanionTracking();
    startCompanionSpeech();
    startCompanionListening();
    companionConversationService_ = new services::CompanionConversationService(this);
    connect(companionConversationService_, &services::CompanionConversationService::responseReady,
            companionSpeechService_, &services::CompanionSpeechService::speak);
    connect(companionSpeechService_, &services::CompanionSpeechService::speechStarted,
            this, [this]() { companionSpeechBusy_ = true; companionListeningService_->setPaused(true); });
    connect(companionSpeechService_, &services::CompanionSpeechService::speechFinished,
            this, [this]() {
                companionSpeechBusy_ = false;
                companionListeningService_->setPaused(false);

                if (companionWakeGreetingPending_) {
                    companionWakeGreetingPending_ = false;
                    companionListeningService_->listen();
                    return;
                }

                // Once HAL has been awakened, keep the conversation open.
                // After each spoken reply, immediately listen for the next turn.
                if (companionConversationSessionActive_) {
                    companionListeningService_->listen();
                }
            });
    connect(companionSpeechService_, &services::CompanionSpeechService::speechFailed,
            this, [this](const QString&) {
                companionSpeechBusy_ = false;
                companionListeningService_->setPaused(false);

                if (companionWakeGreetingPending_) {
                    companionWakeGreetingPending_ = false;
                    companionListeningService_->listen();
                }
            });
    connect(companionListeningService_, &services::CompanionListeningService::listeningStarted,
            this, [this]() { companionListeningBusy_ = true; });

    connect(companionListeningService_, &services::CompanionListeningService::wakeDetected,
            this, [this]() {
                if (companionSpeechBusy_
                    || companionConversationService_->isBusy()
                    || companionWakeGreetingPending_) {
                    return;
                }

                companionConversationSessionActive_ = true;
                companionWakeGreetingPending_ = true;
                companionListeningService_->setPaused(true);

                const int hour = QTime::currentTime().hour();
                QString greeting;

                if (hour >= 5 && hour < 12)
                    greeting = QStringLiteral("Good morning.");
                else if (hour >= 12 && hour < 18)
                    greeting = QStringLiteral("Good afternoon.");
                else
                    greeting = QStringLiteral("Good evening.");

                qCInfo(lcApp) << "HAL wake greeting:" << greeting;
                companionSpeechService_->speak(greeting);
            });
    connect(companionListeningService_, &services::CompanionListeningService::listeningFailed,
            this, [this](const QString&) { companionListeningBusy_ = false; });

    switch (options.mode) {
    case StartupMode::Desktop:
        startDesktop();
        break;
    case StartupMode::Deck:
        startDeck(options.deckScreenIndex);
        break;
    }

    return QApplication::exec();
}

void Application::startDesktop() {
    desktopWindow_ = std::make_unique<desktop::DesktopWindow>();

    // Allow launching Deck Mode (windowed) from the desktop window so the Deck
    // experience is reachable during development without a fullscreen leap.
    connect(desktopWindow_.get(), &desktop::DesktopWindow::launchDeckRequested, this,
            [this]() {
                if (!deckWindow_) {
                    deckWindow_ = std::make_unique<deck::DeckWindow>();
                    connect(deckWindow_.get(), &deck::DeckWindow::exitRequested,
                            deckWindow_.get(), &QWidget::close);
                    // Wire telemetry immediately after construction and before
                    // the window is shown.
                    connectTelemetryToDeck(deckWindow_.get());
                    connectDesktopControlsToDeck(deckWindow_.get());
                    connectDeckActionsToDeck(deckWindow_.get());
                    connectMediaSessionToDeck(deckWindow_.get());
                    connectCompanionTrackingToDeck(deckWindow_.get());
                    connectCompanionSpeechToDeck(deckWindow_.get());
                    connectCompanionListeningToDeck(deckWindow_.get());
                }
                deckWindow_->showWindowed();
                deckWindow_->raise();
                deckWindow_->activateWindow();
            });

    desktopWindow_->show();
    qCInfo(lcApp) << "Started in Desktop mode";
}

void Application::startDeck(int requestedScreenIndex) {
    const QList<QScreen*> screens = QGuiApplication::screens();
    QScreen* target = QGuiApplication::primaryScreen();

    if (requestedScreenIndex == -1) {
        // Explicit "primary/default" request; nothing to validate.
    } else if (requestedScreenIndex == -2) {
        qCWarning(lcApp) << "Invalid --deck-screen index (not a number); "
                            "falling back to primary display";
    } else if (requestedScreenIndex >= 0 && requestedScreenIndex < screens.size()) {
        target = screens.at(requestedScreenIndex);
        qCInfo(lcApp) << "Using screen index" << requestedScreenIndex << ":"
                      << target->name();
    } else {
        qCWarning(lcApp) << "Requested screen index" << requestedScreenIndex
                         << "is out of range (" << screens.size()
                         << "screens); falling back to primary display";
    }

    deckWindow_ = std::make_unique<deck::DeckWindow>();
    connect(deckWindow_.get(), &deck::DeckWindow::exitRequested, deckWindow_.get(),
            &QWidget::close);
    // Wire telemetry immediately after construction and before showing.
    connectTelemetryToDeck(deckWindow_.get());
    connectDesktopControlsToDeck(deckWindow_.get());
    connectDeckActionsToDeck(deckWindow_.get());
    connectMediaSessionToDeck(deckWindow_.get());
    connectCompanionTrackingToDeck(deckWindow_.get());
    connectCompanionSpeechToDeck(deckWindow_.get());
    connectCompanionListeningToDeck(deckWindow_.get());

    deckWindow_->showDeckFullscreen(target);
    qCInfo(lcApp) << "Started in Deck mode";
}

void Application::startTelemetry() {
    if (!providers_.isEmpty()) {
        return;
    }
    // Each concrete service is a QObject child of this Application; Qt owns
    // their lifetimes. They are held through the interface so nothing
    // downstream depends on a concrete type.
    providers_.append(new services::CpuTelemetryService(this));
    providers_.append(new services::MemoryTelemetryService(this));

    for (interfaces::ITelemetryProvider* provider : providers_) {
        provider->start();
    }
    qCInfo(lcApp) << "Telemetry started; providers:" << providers_.size();
}

void Application::startDesktopControls() {
    if (desktopControlService_ == nullptr) {
        desktopControlService_ = new services::DesktopControlService(this);
    }
}

void Application::startDeckActions() {
    if (deckActionService_ == nullptr) {
        deckActionService_ =
            new services::DeckActionService(this);
    }
}

void Application::startMediaSession() {
    if (mediaSessionService_ == nullptr) {
        mediaSessionService_ =
            new services::MediaSessionService(this);
    }
}

void Application::startCompanionTracking() {
    if (companionTrackingService_ == nullptr) {
        companionTrackingService_ =
            new services::CompanionTrackingService(this);
    }
}

void Application::startCompanionSpeech() {
    if (companionSpeechService_ == nullptr) {
        companionSpeechService_ =
            new services::CompanionSpeechService(this);
    }
}

void Application::startCompanionListening() {
    if (companionListeningService_ == nullptr) {
        companionListeningService_ =
            new services::CompanionListeningService(this);
    }
}

void Application::connectDesktopControlsToDeck(deck::DeckWindow* window) {
    if (window == nullptr || desktopControlService_ == nullptr) {
        return;
    }
    connect(window, &deck::DeckWindow::controlRequested, desktopControlService_,
            &services::DesktopControlService::perform);
    connect(desktopControlService_, &services::DesktopControlService::actionCompleted,
            window, &deck::DeckWindow::reportControlResult);
}

void Application::connectDeckActionsToDeck(
    deck::DeckWindow* window) {

    if (window == nullptr || deckActionService_ == nullptr) {
        return;
    }

    connect(
        window,
        &deck::DeckWindow::customControlRequested,
        deckActionService_,
        &services::DeckActionService::perform
    );

    connect(
        deckActionService_,
        &services::DeckActionService::actionCompleted,
        window,
        [window](bool success, const QString& message) {
            Q_UNUSED(success);
            Q_UNUSED(message);
        }
    );

    connect(
        deckActionService_,
        &services::DeckActionService::pageRequested,
        window,
        [window](const QString& pageName) {
            Q_UNUSED(window);
            Q_UNUSED(pageName);
            // Page switching will be wired once PageManager exposes
            // a safe title-based navigation method.
        }
    );
}

void Application::connectMediaSessionToDeck(
    deck::DeckWindow* window) {

    if (window == nullptr || mediaSessionService_ == nullptr)
        return;

    connect(
        mediaSessionService_,
        &services::MediaSessionService::mediaChanged,
        window,
        &deck::DeckWindow::receiveMediaState
    );

    // Prime the UI immediately.
    window->receiveMediaState(
        mediaSessionService_->playerName(),
        mediaSessionService_->title(),
        mediaSessionService_->artist(),
        mediaSessionService_->album(),
        mediaSessionService_->artUrl(),
        mediaSessionService_->playing()
    );
}

void Application::connectCompanionTrackingToDeck(
    deck::DeckWindow* window) {
    if (window == nullptr || companionTrackingService_ == nullptr) {
        return;
    }

    connect(companionTrackingService_,
            &services::CompanionTrackingService::gazeTargetChanged,
            window,
            &deck::DeckWindow::setCompanionGazeTarget);

    connect(companionTrackingService_,
            &services::CompanionTrackingService::trackingLost,
            window,
            &deck::DeckWindow::clearCompanionGazeTarget);
}

void Application::connectCompanionSpeechToDeck(
    deck::DeckWindow* window) {
    if (window == nullptr || companionSpeechService_ == nullptr) {
        return;
    }

    connect(window,
            &deck::DeckWindow::companionSpeechRequested,
            window, [this](const QString& text) {
                if (!companionListeningBusy_ && !companionConversationService_->isBusy()
                    && !companionSpeechBusy_) companionSpeechService_->speak(text);
            });

    connect(companionSpeechService_,
            &services::CompanionSpeechService::speechStarted,
            window,
            [window]() {
                window->setCompanionState(models::CompanionState::Speaking);
            });

    connect(companionSpeechService_,
            &services::CompanionSpeechService::speechFinished,
            window,
            [window]() {
                window->setCompanionState(models::CompanionState::Idle);
            });

    connect(companionSpeechService_,
            &services::CompanionSpeechService::speechFailed,
            window,
            [window](const QString&) {
                window->setCompanionState(models::CompanionState::Alert);
            });
}

void Application::connectCompanionListeningToDeck(
    deck::DeckWindow* window) {
    if (window == nullptr || companionListeningService_ == nullptr) {
        return;
    }

    connect(window,
            &deck::DeckWindow::companionListenRequested,
            window, [this]() {
                if (!companionSpeechBusy_ && !companionConversationService_->isBusy()
                    && !companionListeningBusy_) companionListeningService_->listen();
            });

    connect(companionListeningService_,
            &services::CompanionListeningService::listeningStarted,
            window,
            [window]() {
                window->setCompanionState(
                    models::CompanionState::Listening);
            });

    connect(companionListeningService_,
            &services::CompanionListeningService::transcriptionStarted,
            window,
            [window]() {
                window->setCompanionState(
                    models::CompanionState::Thinking);
            });

    connect(companionListeningService_,
            &services::CompanionListeningService::transcriptionReady,
            window,
            [this, window](const QString& transcript) {
                companionListeningBusy_ = false;
                QString text = transcript.trimmed();
                text.remove(QRegularExpression(QStringLiteral("\\[[^\\]]*\\]|<\\|[^>]*\\|>")));
                text = text.trimmed();

                const QString command = text.toLower()
                    .remove(QRegularExpression(QStringLiteral("[^a-z ]")))
                    .simplified();

                if (command == QStringLiteral("stop listening")
                    || command == QStringLiteral("stop listen")
                    || command == QStringLiteral("go to sleep")
                    || command == QStringLiteral("sleep")
                    || command == QStringLiteral("go to sleep hal")
                    || command == QStringLiteral("good night hal")) {
                    companionConversationSessionActive_ = false;
                    companionWakeGreetingPending_ = false;
                    companionListeningService_->setPaused(true);
                    window->setCompanionState(models::CompanionState::Idle);
                    companionSpeechService_->speak(QStringLiteral("Going to sleep."));
                    return;
                }

                if (text.isEmpty()) {
                    companionListeningService_->setPaused(false);
                    window->setCompanionState(models::CompanionState::Idle);

                    if (companionConversationSessionActive_)
                        companionListeningService_->listen();

                    return;
                }
                window->setCompanionState(models::CompanionState::Thinking);
                companionListeningService_->setPaused(true);
                companionConversationService_->ask(text);
            });

    connect(companionConversationService_,
            &services::CompanionConversationService::errorOccurred,
            window, [this, window](const QString&) {
                companionListeningService_->setPaused(false);
                window->setCompanionState(models::CompanionState::Alert);
            });

    connect(companionListeningService_,
            &services::CompanionListeningService::listeningFailed,
            window,
            [window](const QString&) {
                window->setCompanionState(
                    models::CompanionState::Alert);
            });
}

void Application::connectTelemetryToDeck(deck::DeckWindow* window) {
    if (window == nullptr) {
        return;
    }
    for (interfaces::ITelemetryProvider* provider : providers_) {
        if (provider == nullptr) {
            continue;
        }
        connect(provider, &interfaces::ITelemetryProvider::readingChanged,
                window, &deck::DeckWindow::receiveTelemetry);
        // Deliver the latest known sample for each sensor immediately so a newly
        // shown window is not blank until the next poll. A provider may expose
        // several sensors, so every current sample is primed.
        const QList<models::MetricSample> primed = provider->currentSamples();
        for (const models::MetricSample& sample : primed) {
            window->receiveTelemetry(sample);
        }
    }
}

}  // namespace darkspark::application
