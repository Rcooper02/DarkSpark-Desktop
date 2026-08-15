// SPDX-License-Identifier: GPL-3.0-or-later
#include "application/Application.hpp"

#include <string_view>
#include <memory>
#include <vector>

#include "deck/DeckWindow.hpp"
#include "deck/instruments/AnimationClock.hpp"
#include "deck/instruments/CpuInstrument.hpp"
#include "deck/instruments/CpuInstrumentModelAdapter.hpp"
#include "deck/instruments/InstrumentPreviewPage.hpp"
#include "deck/instruments/InstrumentModelFanout.hpp"
#include "deck/instruments/GpuInstrument.hpp"
#include "deck/instruments/GpuInstrumentModelAdapter.hpp"
#include "services/GpuTelemetryService.hpp"
#include "services/GpuThermalService.hpp"
#include "services/GpuVramService.hpp"
#include "deck/pages/CommandDeckPage.hpp"
#include "deck/layout/LayoutPersistenceService.hpp"
#include "deck/layout/DeckLayoutCollection.hpp"
#include "deck/navigation/PageManager.hpp"
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

// Diagnostic-only category for the GPU composition-root wiring (provider start,
// priming, and sample->adapter->instrument flow). OFF by default; enable with
// QT_LOGGING_RULES="darkspark.wiring.gpu=true". Throttled to model changes.
Q_LOGGING_CATEGORY(lcGpuWiring, "darkspark.wiring.gpu")
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

    // Resolve the multi-page COLLECTION from persistence before building pages:
    // loads a valid v2 collection, migrates a legacy v1 file, or writes+returns
    // the compiled 3-page default on first run / self-recovers from a bad file.
    // Pages receive resolved layouts and know nothing about JSON or files.
    auto layoutPersistence =
        std::make_shared<deck::layout::LayoutPersistenceService>();
    const deck::layout::DeckLayoutCollection collection =
        layoutPersistence->loadCollectionOrDefault();

    // A PageManager hosts every page and owns navigation (swipe/keyboard) and
    // the page indicator. Each page owns its own instrument QWidget instances;
    // Application collects every live instance and fans one subsystem model out
    // to all matching views. System remains the only editable/persisted page in
    // this batch, but telemetry is no longer tied to System-page pointers.
    auto* pageManager = new deck::navigation::PageManager(window.get());
    deck::pages::CommandDeckPage* systemPage = nullptr;
    int systemPageId = collection.activePageId;
    int activeIndex = 0;

    // Every page owns its own instrument QWidget instances, but all instances
    // of a subsystem are views of ONE telemetry/model stream. Collect the live
    // views as pages are built so one adapter update can fan out to every page
    // that contains that WidgetId. Empty pages simply contribute no targets.
    std::vector<deck::instruments::CpuInstrument*> cpuTargets;
    std::vector<deck::instruments::GpuInstrument*> gpuTargets;
    std::vector<deck::instruments::MemoryInstrument*> memoryTargets;
    std::vector<deck::instruments::CoolingInstrument*> coolingTargets;
    std::vector<deck::instruments::StorageInstrument*> storageTargets;
    std::vector<deck::instruments::NetworkInstrument*> networkTargets;

    const auto collectPageTargets =
        [&](deck::pages::CommandDeckPage* deckPage) {
            if (deckPage == nullptr) {
                return;
            }
            if (auto* instrument = deckPage->primaryInstrument();
                instrument != nullptr) {
                cpuTargets.push_back(instrument);
            }
            if (auto* instrument = deckPage->gpuInstrument();
                instrument != nullptr) {
                gpuTargets.push_back(instrument);
            }
            if (auto* instrument = deckPage->memoryInstrument();
                instrument != nullptr) {
                memoryTargets.push_back(instrument);
            }
            if (auto* instrument = deckPage->coolingInstrument();
                instrument != nullptr) {
                coolingTargets.push_back(instrument);
            }
            if (auto* instrument = deckPage->storageInstrument();
                instrument != nullptr) {
                storageTargets.push_back(instrument);
            }
            if (auto* instrument = deckPage->networkInstrument();
                instrument != nullptr) {
                networkTargets.push_back(instrument);
            }
        };
    for (int i = 0; i < static_cast<int>(collection.pages.size()); ++i) {
        const deck::layout::DeckPageDefinition& def = collection.pages[i];
        auto* deckPage =
            new deck::pages::CommandDeckPage(def.layout, pageManager);
        pageManager->addPage(deckPage);
        collectPageTargets(deckPage);
        // The System page is the one that actually contains instruments (its
        // layout places CPU). Identify it by a placed CPU widget so telemetry
        // binds to the page that has the accessors.
        if (systemPage == nullptr
            && deckPage->primaryInstrument() != nullptr) {
            systemPage = deckPage;
            systemPageId = def.pageId;
        }
        if (def.pageId == collection.activePageId) {
            activeIndex = i;
        }
    }
    outer->addWidget(pageManager);

    // Restore the persisted active page, then persist future page switches.
    // activePageChanged fires only on real navigation (not per frame), so the
    // file is rewritten only on meaningful state changes.
    pageManager->goToPage(activeIndex);
    QObject::connect(
        pageManager, &deck::navigation::PageManager::activePageChanged,
        pageManager, [layoutPersistence, collection](int index) {
            if (index >= 0
                && index < static_cast<int>(collection.pages.size())) {
                layoutPersistence->saveActivePage(
                    collection.pages[static_cast<std::size_t>(index)].pageId);
            }
        });

    // System remains the Edit Mode persistence anchor. If no persisted page had
    // a Primary instrument (should not happen with the compiled default), add a
    // standalone System page and collect its instrument views too.
    deck::pages::CommandDeckPage* page = systemPage;
    if (page == nullptr) {
        page = new deck::pages::CommandDeckPage(
            deck::layout::defaultCommandDeckLayout(), window.get());
        outer->addWidget(page);
        collectPageTargets(page);
    }

    // Edit Mode persistence seam: when the System page commits an edit (Save),
    // update ONLY that page's layout in the persisted collection and write it.
    // The page emits its new layout and stays unaware of collections/files; the
    // composition root -- which already owns the collection, the service, and
    // each page's id -- does the collection update and save. activePageId is
    // untouched. Cancel emits nothing, so this never fires and nothing is
    // written. Persistence happens on Save only, never on individual moves.
    {
        QObject::connect(
            page, &deck::pages::CommandDeckPage::layoutCommitted, page,
            [layoutPersistence, systemPageId](
                const deck::layout::DeckLayout& edited) {
                deck::layout::DeckLayoutCollection current =
                    layoutPersistence->loadCollectionOrDefault();
                bool updated = false;
                for (deck::layout::DeckPageDefinition& def : current.pages) {
                    if (def.pageId == systemPageId) {
                        def.layout = edited;
                        updated = true;
                        break;
                    }
                }
                if (updated && deck::layout::isValidCollection(current)) {
                    layoutPersistence->saveCollection(current);
                }
            });
    }

    // One shared animation clock for the entire Command Deck (parented to the
    // window). Every live instrument view on every page uses this same clock;
    // hidden pages unsubscribe through their instruments' show/hide lifecycle,
    // so there is still exactly one animation QTimer for the deck.
    auto* animationClock =
        new deck::instruments::AnimationClock(window.get());
    for (auto* instrument : cpuTargets) {
        instrument->setAnimationClock(animationClock);
        instrument->setPersonality(
            deck::instruments::InstrumentPersonality::core());
    }
    for (auto* instrument : gpuTargets) {
        instrument->setAnimationClock(animationClock);
    }
    for (auto* instrument : memoryTargets) {
        instrument->setAnimationClock(animationClock);
    }
    for (auto* instrument : coolingTargets) {
        instrument->setAnimationClock(animationClock);
    }
    for (auto* instrument : storageTargets) {
        instrument->setAnimationClock(animationClock);
    }
    for (auto* instrument : networkTargets) {
        instrument->setAnimationClock(animationClock);
    }

    // CPU is the reference personality this milestone: "The Core". Every CPU
    // view gets that same identity; the other subsystem views stay neutral.

    connect(new QShortcut(QKeySequence(Qt::Key_Escape), window.get()),
            &QShortcut::activated, window.get(), &QWidget::close);

    // Telemetry wiring lives HERE, in the composition root -- not in pages.
    // Each subsystem has one provider/adapter pipeline; when its model changes,
    // fanOutInstrumentModel publishes that model to every live page view of that
    // subsystem. Pages remain telemetry-independent and duplicate widgets do not
    // duplicate providers. Providers are owned by the window; adapters are kept
    // alive by the sample-handler connections.
    auto adapter =
        std::make_shared<deck::instruments::CpuInstrumentModelAdapter>();
    auto applySample = [adapter, cpuTargets](const models::MetricSample& sample) {
        if (adapter->apply(sample)) {
            deck::instruments::fanOutInstrumentModel(adapter->model(),
                                                      cpuTargets);
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

    // GPU wiring: one provider/adapter stream fans out to every GPU view across
    // the Command Deck pages. The pages remain telemetry-independent.
    auto gpuAdapter =
        std::make_shared<deck::instruments::GpuInstrumentModelAdapter>();
    auto applyGpuSample =
        [gpuAdapter, gpuTargets](const models::MetricSample& sample) {
            const bool changed = gpuAdapter->apply(sample);
            // Diagnostic: a sample arrived; did it change the model? (Logged
            // only when it did, so a steady stream doesn't flood.)
            if (changed) {
                qCDebug(lcGpuWiring)
                    << "gpu sample metric=" << static_cast<int>(sample.id())
                    << "state=" << static_cast<int>(sample.state())
                    << "-> model updated";
                deck::instruments::fanOutInstrumentModel(gpuAdapter->model(),
                                                          gpuTargets);
            }
        };

    auto* gpuUtil = new services::GpuTelemetryService(window.get());
    auto* gpuThermal = new services::GpuThermalService(window.get());
    auto* gpuVram = new services::GpuVramService(window.get());
    for (interfaces::ITelemetryProvider* provider :
         {static_cast<interfaces::ITelemetryProvider*>(gpuUtil),
          static_cast<interfaces::ITelemetryProvider*>(gpuThermal),
          static_cast<interfaces::ITelemetryProvider*>(gpuVram)}) {
        connect(provider, &interfaces::ITelemetryProvider::readingChanged,
                window.get(), applyGpuSample);
        provider->start();
        const QList<models::MetricSample> primed = provider->currentSamples();
        qCInfo(lcGpuWiring)
            << "gpu provider started and primed" << primed.size() << "sample(s)";
        for (const models::MetricSample& sample : primed) {
            applyGpuSample(sample);
        }
    }

    // Memory wiring: one MemoryTelemetryService emits the joined samples and
    // one adapter builds the model, which is then published to every Memory view.
    auto memoryAdapter =
        std::make_shared<deck::instruments::MemoryInstrumentModelAdapter>();
    auto applyMemorySample =
        [memoryAdapter, memoryTargets](const models::MetricSample& sample) {
            if (memoryAdapter->apply(sample)) {
                deck::instruments::fanOutInstrumentModel(memoryAdapter->model(),
                                                          memoryTargets);
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

    // Cooling wiring: the provider/adapter pair remains singular while its
    // presentation model is published to every Cooling view.
    auto coolingAdapter =
        std::make_shared<deck::instruments::CoolingInstrumentModelAdapter>();
    auto applyCoolingSample =
        [coolingAdapter, coolingTargets](const models::MetricSample& sample) {
            if (coolingAdapter->apply(sample)) {
                deck::instruments::fanOutInstrumentModel(coolingAdapter->model(),
                                                          coolingTargets);
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

    // Storage wiring: one aggregate provider/adapter stream fans its joined
    // presentation model out to every Storage view.
    auto storageAdapter =
        std::make_shared<deck::instruments::StorageInstrumentModelAdapter>();
    auto applyStorageSample =
        [storageAdapter, storageTargets](const models::MetricSample& sample) {
            if (storageAdapter->apply(sample)) {
                deck::instruments::fanOutInstrumentModel(storageAdapter->model(),
                                                          storageTargets);
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

    // Network wiring: the service/adapter remain singular, interface identity
    // is retained from the sample key, and the resulting model is published to
    // every Network view.
    auto networkAdapter =
        std::make_shared<deck::instruments::NetworkInstrumentModelAdapter>();
    auto applyNetworkSample =
        [networkAdapter, networkTargets](const models::MetricSample& sample) {
            if (networkAdapter->apply(sample)) {
                auto model = networkAdapter->model();
                // Populate the retained interface identity from the stable key
                // ("net:<iface>"); the flags remain future-UI scaffolding in V1.
                const std::string key = sample.sensorKey();
                constexpr std::string_view kPrefix = "net:";
                if (key.rfind(kPrefix, 0) == 0) {
                    model.interfaceName = key.substr(kPrefix.size());
                }
                deck::instruments::fanOutInstrumentModel(model, networkTargets);
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
