// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_APPLICATION_APPLICATION_HPP
#define DARKSPARK_APPLICATION_APPLICATION_HPP

#include <memory>

#include <QObject>
#include <QStringList>

class QApplication;

namespace darkspark::desktop {
class DesktopWindow;
}
namespace darkspark::deck {
class DeckWindow;
}
namespace darkspark::interfaces {
class ITelemetryProvider;
}

namespace darkspark::application {

/// Startup mode selected from the command line.
enum class StartupMode {
    Desktop,  ///< standard development window (default)
    Deck      ///< Deck Mode fullscreen
};

/// Parsed, validated command-line options.
struct LaunchOptions {
    StartupMode mode = StartupMode::Desktop;
    /// Requested Qt screen index for Deck Mode, or -1 for "primary/default".
    int deckScreenIndex = -1;
};

/// Composition root for DarkSpark Desktop.
///
/// Application owns the top-level windows and wires them together. It parses
/// the command line into LaunchOptions, applies the Legacy theme, selects the
/// startup mode, and resolves the target display with a safe fallback to the
/// primary screen for invalid selections. It creates only what the chosen mode
/// needs.
///
/// This class deliberately contains no domain/service logic (there are no
/// services in Deck-0). Ownership: constructed on the stack in main() and given
/// a reference to the QApplication it does not own. Threading: GUI thread only.
class Application : public QObject {
    Q_OBJECT

public:
    explicit Application(QApplication& qtApp);
    ~Application() override;

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    /// Parse command-line arguments into validated launch options.
    ///
    /// Recognized:
    ///   (none)                  -> Desktop mode
    ///   --deck                  -> Deck Mode on default/primary screen
    ///   --deck-screen <index>   -> Deck Mode on the given Qt screen index
    ///
    /// Unparseable or out-of-range values are handled at run() time with a
    /// logged warning and safe fallback, not by aborting.
    static LaunchOptions parseArguments(const QStringList& arguments);

    /// Apply theme, select mode, show the appropriate window. Returns a process
    /// exit code (0 on success).
    int run(const LaunchOptions& options);

private:
    void startDesktop();
    void startDeck(int requestedScreenIndex);

    /// Create the telemetry provider (owned via Qt parenting) and begin
    /// sampling. Called once from run() before mode selection, so telemetry is
    /// available immediately whenever a Deck window appears.
    void startTelemetry();

    /// Connect the telemetry provider to a freshly constructed DeckWindow.
    ///
    /// Both DeckWindow creation paths (direct Deck startup and the launch
    /// request from the desktop window) route through this helper so the wiring
    /// is identical. The window is used as the connection context object, so
    /// the connection is removed automatically if the window is destroyed.
    void connectTelemetryToDeck(deck::DeckWindow* window);

    QApplication& qtApp_;
    std::unique_ptr<desktop::DesktopWindow> desktopWindow_;
    std::unique_ptr<deck::DeckWindow> deckWindow_;
    /// Non-owning view of the telemetry provider. The concrete service is a
    /// QObject child of this Application, so Qt owns its lifetime. Held through
    /// the interface so the composition root does not depend on the concrete
    /// service type beyond construction.
    interfaces::ITelemetryProvider* telemetry_ = nullptr;
};

}  // namespace darkspark::application

#endif  // DARKSPARK_APPLICATION_APPLICATION_HPP
