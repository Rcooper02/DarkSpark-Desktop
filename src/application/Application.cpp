// SPDX-License-Identifier: GPL-3.0-or-later
#include "application/Application.hpp"

#include <string_view>

#include "deck/DeckWindow.hpp"
#include "deck/instruments/AnimationClock.hpp"
#include "deck/instruments/CpuInstrument.hpp"
#include "deck/instruments/CpuInstrumentModelAdapter.hpp"
#include "deck/instruments/InstrumentPreviewPage.hpp"
#include "deck/instruments/GpuInstrument.hpp"
#include "deck/instruments/GpuInstrumentModelAdapter.hpp"
#include "services/GpuTelemetryService.hpp"
#include "services/GpuThermalService.hpp"
#include "deck/pages/CommandDeckPage.hpp"
#include "desktop/DesktopWindow.hpp"
#include "interfaces/ITelemetryProvider.hpp"
#include "models/MetricSample.hpp"
#include "services/CpuTelemetryService.hpp"
#include "services/CpuThermalService.hpp"
#include "deck/instruments/MemoryInstrument.hpp"
#include "deck/instruments/MemoryInstrumentModelAdapter.hpp"
#include "deck/instruments/CoolingInstrument.hpp"
#include "deck/instruments/CoolingInstrumentModelAdapter.hpp"
#include "deck/instruments/StorageInstrument.hpp"
#include "deck/instruments/NetworkInstrument.hpp"
#include "deck/instruments/StorageInstrumentModelAdapter.hpp"
#include "deck/instruments/NetworkInstrumentModelAdapter.hpp"
#include "services/MemoryTelemetryService.hpp"
#include "services/CoolingTelemetryService.hpp"
#include "services/StorageTelemetryService.hpp"
#include "services/NetworkTelemetryService.hpp"
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
        if (arg == QStringLiteral("--command-deck")) {
            options.mode = StartupMode::CommandDeck;
        } else if (arg == QStringLiteral("--instrument-preview")) {
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
    case StartupMode::CommandDeck:
        startCommandDeck();
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
    auto* page = new deck::instruments::InstrumentPreviewPage(
        deck::instruments::InstrumentPreviewPage::Layout::SideBySide,
        window.get());
    outer->addWidget(page);

    connect(new QShortcut(QKeySequence(Qt::Key_Escape), window.get()),
            &QShortcut::activated, window.get(), &QWidget::close);

    // Bind live telemetry to the preview using the real pipeline. The preview
    // route runs its own CPU utilization and thermal providers (independent of
    // the dashboard's providers_, so the preview is fully self-contained), and
    // connects each provider's readingChanged to the page, exactly as the deck
    // path connects providers to the deck window. The page owns the adapter that
    // turns samples into the instruments' presentation model.
    auto* utilization = new services::CpuTelemetryService(window.get());
    auto* thermal = new services::CpuThermalService(window.get());
    for (interfaces::ITelemetryProvider* provider :
         {static_cast<interfaces::ITelemetryProvider*>(utilization),
          static_cast<interfaces::ITelemetryProvider*>(thermal)}) {
        connect(provider, &interfaces::ITelemetryProvider::readingChanged, page,
                &deck::instruments::InstrumentPreviewPage::receiveTelemetry);
        provider->start();
        // Immediate initialization: prime the page from the providers' current
        // samples right now, before the window is shown, so the very first
        // painted frame already reflects real telemetry state rather than a
        // blank widget. Priming happens for every sensor a provider exposes.
        //
        // Note on honesty: the first primed samples are Unavailable by design --
        // utilization is a rate that needs two reads to produce a value, and
        // thermal reads values on its first poll -- so the instrument shows its
        // correct Absent presentation (placeholder, dormant conduit) from frame
        // zero and then eases into live values as the providers produce them.
        // We deliberately do not fabricate a frame-zero number. Polling then
        // continues normally on each provider's timer.
        const QList<models::MetricSample> primed = provider->currentSamples();
        for (const models::MetricSample& sample : primed) {
            page->receiveTelemetry(sample);
        }
    }

    window->showFullScreen();
    instrumentPreviewWindow_ = std::move(window);
    qCInfo(lcApp) << "Started in Instrument Preview mode (live telemetry)";
}

void Application::startCommandDeck() {
    // The real Command Deck composition, in its own fullscreen host. A new page
    // that coexists with the existing card dashboard and the developer preview
    // -- it replaces neither.
    auto window = std::make_unique<QWidget>();
    window->setObjectName(themes::LegacyTheme::pageObjectName());
    window->setStyleSheet(
        QStringLiteral("QWidget#%1 { background-color: %2; }")
            .arg(themes::LegacyTheme::pageObjectName(),
                 themes::LegacyTheme::backgroundBase().name()));

    auto* outer = new QVBoxLayout(window.get());
    outer->setContentsMargins(0, 0, 0, 0);
    auto* page = new deck::pages::CommandDeckPage(window.get());
    outer->addWidget(page);

    // One shared animation clock for the entire Command Deck (parented to the
    // window). It replaces the six former per-instrument timers: every
    // instrument is driven by this single clock, so there is exactly one
    // animation QTimer for the deck. Each instrument subscribes/unsubscribes on
    // show/hide, and the clock stops entirely when nothing needs animation.
    auto* animationClock =
        new deck::instruments::AnimationClock(window.get());
    page->primaryInstrument()->setAnimationClock(animationClock);
    page->gpuInstrument()->setAnimationClock(animationClock);
    page->memoryInstrument()->setAnimationClock(animationClock);
    page->coolingInstrument()->setAnimationClock(animationClock);
    page->storageInstrument()->setAnimationClock(animationClock);
    page->networkInstrument()->setAnimationClock(animationClock);

    // CPU is the reference personality this milestone: "The Core". The other
    // five instruments keep the neutral personality (byte-identical visuals).
    page->primaryInstrument()->setPersonality(
        deck::instruments::InstrumentPersonality::core());

    connect(new QShortcut(QKeySequence(Qt::Key_Escape), window.get()),
            &QShortcut::activated, window.get(), &QWidget::close);

    // Telemetry wiring lives HERE, in the composition root -- not in the page.
    // The page composes instruments and regions and knows nothing about
    // telemetry; Application owns the adapter and provider wiring and drives the
    // page's primary instrument:
    //
    //     provider -> adapter -> page->primaryInstrument()->setModel()
    //
    // A future subsystem instrument would be bound the same way, so the page
    // never becomes a telemetry coordinator. Providers are owned by the window,
    // independent of the dashboard's providers_, so the Command Deck is
    // self-contained. The adapter is a plain value type; a shared_ptr captured
    // by the sample handler ties its lifetime to the connections (and window).
    auto* primary = page->primaryInstrument();
    auto adapter =
        std::make_shared<deck::instruments::CpuInstrumentModelAdapter>();
    auto applySample = [adapter, primary](const models::MetricSample& sample) {
        if (adapter->apply(sample)) {
            primary->setModel(adapter->model());
        }
    };

    auto* utilization = new services::CpuTelemetryService(window.get());
    auto* thermal = new services::CpuThermalService(window.get());
    for (interfaces::ITelemetryProvider* provider :
         {static_cast<interfaces::ITelemetryProvider*>(utilization),
          static_cast<interfaces::ITelemetryProvider*>(thermal)}) {
        connect(provider, &interfaces::ITelemetryProvider::readingChanged,
                window.get(), applySample);
        provider->start();
        // Immediate initialization: prime from current samples before the
        // window is shown, so the first painted frame reflects real state.
        const QList<models::MetricSample> primed = provider->currentSamples();
        for (const models::MetricSample& sample : primed) {
            applySample(sample);
        }
    }

    // GPU wiring, the second live subsystem, bound exactly like CPU from the
    // composition root: gpuProvider -> gpuAdapter -> page->gpuInstrument().
    // GPU has its own providers, its own adapter, and its own instrument type;
    // the page stays telemetry-independent. This is the reference pattern for
    // every future subsystem.
    auto* gpu = page->gpuInstrument();
    auto gpuAdapter =
        std::make_shared<deck::instruments::GpuInstrumentModelAdapter>();
    auto applyGpuSample =
        [gpuAdapter, gpu](const models::MetricSample& sample) {
            if (gpuAdapter->apply(sample)) {
                gpu->setModel(gpuAdapter->model());
            }
        };

    auto* gpuUtil = new services::GpuTelemetryService(window.get());
    auto* gpuThermal = new services::GpuThermalService(window.get());
    for (interfaces::ITelemetryProvider* provider :
         {static_cast<interfaces::ITelemetryProvider*>(gpuUtil),
          static_cast<interfaces::ITelemetryProvider*>(gpuThermal)}) {
        connect(provider, &interfaces::ITelemetryProvider::readingChanged,
                window.get(), applyGpuSample);
        provider->start();
        const QList<models::MetricSample> primed = provider->currentSamples();
        for (const models::MetricSample& sample : primed) {
            applyGpuSample(sample);
        }
    }

    // Memory wiring, the third live subsystem, bound with the same reference
    // pattern: memoryProvider -> memoryAdapter -> page->memoryInstrument(). The
    // single MemoryTelemetryService emits three joined samples (utilization plus
    // used/total bytes); the adapter joins them into one model. The page stays
    // telemetry-independent.
    auto* memory = page->memoryInstrument();
    auto memoryAdapter =
        std::make_shared<deck::instruments::MemoryInstrumentModelAdapter>();
    auto applyMemorySample =
        [memoryAdapter, memory](const models::MetricSample& sample) {
            if (memoryAdapter->apply(sample)) {
                memory->setModel(memoryAdapter->model());
            }
        };

    auto* memoryProvider = new services::MemoryTelemetryService(window.get());
    connect(memoryProvider, &interfaces::ITelemetryProvider::readingChanged,
            window.get(), applyMemorySample);
    memoryProvider->start();
    const QList<models::MetricSample> memoryPrimed =
        memoryProvider->currentSamples();
    for (const models::MetricSample& sample : memoryPrimed) {
        applyMemorySample(sample);
    }

    // Cooling wiring, the fourth live subsystem, bound with the same reference
    // pattern: coolingProvider -> coolingAdapter -> page->coolingInstrument().
    // The provider aggregates cooling sensor providers, selects roles, and emits
    // role-based samples; the adapter joins them. The page stays
    // telemetry-independent.
    auto* cooling = page->coolingInstrument();
    auto coolingAdapter =
        std::make_shared<deck::instruments::CoolingInstrumentModelAdapter>();
    auto applyCoolingSample =
        [coolingAdapter, cooling](const models::MetricSample& sample) {
            if (coolingAdapter->apply(sample)) {
                cooling->setModel(coolingAdapter->model());
            }
        };

    auto* coolingProvider = new services::CoolingTelemetryService(window.get());
    connect(coolingProvider, &interfaces::ITelemetryProvider::readingChanged,
            window.get(), applyCoolingSample);
    coolingProvider->start();
    const QList<models::MetricSample> coolingPrimed =
        coolingProvider->currentSamples();
    for (const models::MetricSample& sample : coolingPrimed) {
        applyCoolingSample(sample);
    }

    // Storage wiring, the fifth live subsystem: storageProvider -> storageAdapter
    // -> page->storageInstrument(). The service aggregates filesystem/NVMe/
    // diskstats providers, applies the selection policies, and emits six
    // role-based samples; the adapter joins them (retaining temperature and
    // throughput even though the V1 face shows only utilization + used/total).
    auto* storage = page->storageInstrument();
    auto storageAdapter =
        std::make_shared<deck::instruments::StorageInstrumentModelAdapter>();
    auto applyStorageSample =
        [storageAdapter, storage](const models::MetricSample& sample) {
            if (storageAdapter->apply(sample)) {
                storage->setModel(storageAdapter->model());
            }
        };

    auto* storageProvider = new services::StorageTelemetryService(window.get());
    connect(storageProvider, &interfaces::ITelemetryProvider::readingChanged,
            window.get(), applyStorageSample);
    storageProvider->start();
    const QList<models::MetricSample> storagePrimed =
        storageProvider->currentSamples();
    for (const models::MetricSample& sample : storagePrimed) {
        applyStorageSample(sample);
    }

    // Network wiring, the sixth live subsystem: networkProvider -> networkAdapter
    // -> page->networkInstrument(). The service aggregates network interface
    // providers, applies the deterministic active-interface selection policy, and
    // emits five role-based samples; the adapter joins them (retaining cumulative
    // bytes and link state even though the V1 face shows only download + upload).
    // Retained interface identity is populated from the sample key ("net:<iface>")
    // so the page needs no telemetry knowledge.
    auto* network = page->networkInstrument();
    auto networkAdapter =
        std::make_shared<deck::instruments::NetworkInstrumentModelAdapter>();
    auto applyNetworkSample =
        [networkAdapter, network](const models::MetricSample& sample) {
            if (networkAdapter->apply(sample)) {
                auto model = networkAdapter->model();
                // Populate the retained interface identity from the stable key
                // ("net:<iface>"); the flags remain future-UI scaffolding in V1.
                const std::string key = sample.sensorKey();
                constexpr std::string_view kPrefix = "net:";
                if (key.rfind(kPrefix, 0) == 0) {
                    model.interfaceName = key.substr(kPrefix.size());
                }
                network->setModel(model);
            }
        };

    auto* networkProvider = new services::NetworkTelemetryService(window.get());
    connect(networkProvider, &interfaces::ITelemetryProvider::readingChanged,
            window.get(), applyNetworkSample);
    networkProvider->start();
    const QList<models::MetricSample> networkPrimed =
        networkProvider->currentSamples();
    for (const models::MetricSample& sample : networkPrimed) {
        applyNetworkSample(sample);
    }

    window->showFullScreen();
    commandDeckWindow_ = std::move(window);
    qCInfo(lcApp) << "Started in Command Deck mode (live CPU + GPU + Memory + Cooling + Storage + Network telemetry)";
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
