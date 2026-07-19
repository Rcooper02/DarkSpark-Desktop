// SPDX-License-Identifier: GPL-3.0-or-later
#include "application/Application.hpp"

#include "deck/DeckWindow.hpp"
#include "desktop/DesktopWindow.hpp"
#include "interfaces/ITelemetryProvider.hpp"
#include "models/MetricSample.hpp"
#include "services/CpuTelemetryService.hpp"
#include "themes/LegacyTheme.hpp"

#include <QApplication>
#include <QGuiApplication>
#include <QList>
#include <QLoggingCategory>
#include <QScreen>
#include <QStringList>

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

    deckWindow_->showDeckFullscreen(target);
    qCInfo(lcApp) << "Started in Deck mode";
}

void Application::startTelemetry() {
    if (telemetry_ != nullptr) {
        return;
    }
    // The concrete service is a QObject child of this Application; Qt owns its
    // lifetime. It is held through the interface so nothing downstream depends
    // on the concrete type.
    auto* service = new services::CpuTelemetryService(this);
    telemetry_ = service;
    telemetry_->start();
    qCInfo(lcApp) << "CPU telemetry started";
}

void Application::connectTelemetryToDeck(deck::DeckWindow* window) {
    if (telemetry_ == nullptr || window == nullptr) {
        return;
    }
    connect(telemetry_, &interfaces::ITelemetryProvider::readingChanged, window,
            &deck::DeckWindow::receiveTelemetry);
    // Deliver the latest known sample immediately so a newly shown window is
    // not blank until the next poll.
    window->receiveTelemetry(telemetry_->currentSample());
}

}  // namespace darkspark::application
