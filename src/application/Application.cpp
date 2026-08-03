// SPDX-License-Identifier: GPL-3.0-or-later
#include "application/Application.hpp"

#include "deck/DeckWindow.hpp"
#include "deck/instruments/InstrumentPreviewPage.hpp"
#include "desktop/DesktopWindow.hpp"
#include "interfaces/ITelemetryProvider.hpp"
#include "models/MetricSample.hpp"
#include "services/CpuTelemetryService.hpp"
#include "services/CpuThermalService.hpp"
#include "services/MemoryTelemetryService.hpp"
#include "themes/LegacyTheme.hpp"

#include <QApplication>
#include <QGuiApplication>
#include <QList>
#include <QLoggingCategory>
#include <QKeySequence>
#include <QScreen>
#include <QShortcut>
#include <QVBoxLayout>
#include <QWidget>
#include <QString>
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
        if (arg == QStringLiteral("--instrument-preview")) {
            options.mode = StartupMode::InstrumentPreview;
        } else if (arg == QStringLiteral("--deck")) {
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
    case StartupMode::InstrumentPreview:
        startInstrumentPreview();
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

void Application::startInstrumentPreview() {
    // A minimal, isolated host window for the prototype: themed background, the
    // preview page centered in it. Deliberately not the full DeckWindow chrome
    // (no pages/navigation) so the instrument is judged on its own.
    auto window = std::make_unique<QWidget>();
    window->setObjectName(themes::LegacyTheme::pageObjectName());
    window->setStyleSheet(
        QStringLiteral("QWidget#%1 { background-color: %2; }")
            .arg(themes::LegacyTheme::pageObjectName(),
                 themes::LegacyTheme::backgroundBase().name()));

    auto* outer = new QVBoxLayout(window.get());
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(new deck::instruments::InstrumentPreviewPage(
        deck::instruments::InstrumentPreviewPage::Layout::SideBySide,
        window.get()));

    connect(new QShortcut(QKeySequence(Qt::Key_Escape), window.get()),
            &QShortcut::activated, window.get(), &QWidget::close);

    window->showFullScreen();
    instrumentPreviewWindow_ = std::move(window);
    qCInfo(lcApp) << "Started in Instrument Preview mode";
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
    providers_.append(new services::CpuThermalService(this));

    for (interfaces::ITelemetryProvider* provider : providers_) {
        provider->start();
    }
    qCInfo(lcApp) << "Telemetry started; providers:" << providers_.size();

    // Readable diagnostic logging for CPU temperature: after start(), the
    // thermal provider has discovered its sensors, so its primed samples name
    // the logical identities (package, ccd1, ccd2, ...). Temperature has no
    // dashboard binding in T7A, so this log is the integration's visible proof.
    logTemperatureSamples();
}

void Application::logTemperatureSamples() {
    for (interfaces::ITelemetryProvider* provider : providers_) {
        if (provider == nullptr) {
            continue;
        }
        const QList<models::MetricSample> samples = provider->currentSamples();
        for (const models::MetricSample& sample : samples) {
            if (sample.id() != models::MetricId::CpuTemperature) {
                continue;
            }
            const QString key = QString::fromStdString(sample.sensorKey());
            if (sample.state() == models::MetricState::Fresh
                && sample.value().has_value()) {
                qCInfo(lcApp).noquote()
                    << QStringLiteral("[CPU Thermal] %1 = %2\u00B0C (primed)")
                           .arg(key)
                           .arg(QString::number(*sample.value(), 'f', 0));
            } else {
                qCInfo(lcApp).noquote()
                    << QStringLiteral(
                           "[CPU Thermal] %1 discovered, awaiting first reading")
                           .arg(key);
            }
        }
    }
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
