// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_DECK_INSTRUMENTS_ANIMATIONCLOCK_HPP
#define DARKSPARK_DECK_INSTRUMENTS_ANIMATIONCLOCK_HPP

#include <QObject>

#include <cstdint>
#include <vector>

class QTimer;

namespace darkspark::deck::instruments {

/// A subscriber to the shared animation clock. Instruments implement this and
/// register with the clock; the clock ticks them all from a SINGLE timer instead
/// of each owning its own. `advance` receives the elapsed time since the last
/// tick (seconds) and the absolute clock time (seconds); implementations use
/// these to progress interpolation and ambient personality phase, then repaint
/// only if something visibly changed.
///
/// `wantsContinuousAnimation` lets the clock stop entirely when nothing needs
/// motion: an instrument returns true while interpolating or while its
/// personality has live idle motion, and false when fully settled and static.
/// When every subscriber is settled (or hidden), the clock's timer stops, so a
/// dormant deck costs nothing.
class AnimationTickable {
public:
    virtual ~AnimationTickable() = default;

    /// Progress animation by `deltaSeconds`; `clockSeconds` is the monotonic
    /// absolute time. Called only while the clock is running and only for
    /// subscribers that are currently animating.
    virtual void advance(double deltaSeconds, double clockSeconds) = 0;

    /// Whether this subscriber currently needs continued ticks (interpolating,
    /// or running idle personality motion). When all subscribers return false,
    /// the clock stops until something changes and calls requestAnimation().
    [[nodiscard]] virtual bool wantsContinuousAnimation() const = 0;
};

/// One shared ~60 FPS animation driver for the whole Command Deck.
///
/// Owned by Application (composition root). Replaces the six per-instrument
/// QTimers: instruments subscribe/unsubscribe (typically on show/hide) and the
/// clock ticks all active subscribers from a single QTimer. The frame rate is a
/// fixed display cap and is fully DECOUPLED from telemetry polling (~1 s):
/// telemetry sets interpolation targets, the clock drives motion toward and
/// around them.
///
/// Efficiency:
///  - a single timer for the entire deck (not one per instrument),
///  - the timer runs only while at least one subscriber wantsContinuousAnimation,
///  - subscribers that are hidden unsubscribe and cost nothing,
///  - requestAnimation() restarts the timer when new telemetry arrives.
class AnimationClock : public QObject {
    Q_OBJECT

public:
    explicit AnimationClock(QObject* parent = nullptr);
    ~AnimationClock() override;

    /// Register/unregister a subscriber. Safe to call at any time; registering
    /// wakes the clock so the new subscriber gets ticked if it wants animation.
    void subscribe(AnimationTickable* tickable);
    void unsubscribe(AnimationTickable* tickable);

    /// Nudge the clock awake (e.g. an instrument just received new telemetry and
    /// now wants to interpolate). Idempotent; starts the timer if stopped.
    void requestAnimation();

    /// Global pause/resume for deck dormancy (window hidden/minimized). While
    /// paused, no ticks are delivered and the timer is stopped.
    void setPaused(bool paused);
    [[nodiscard]] bool isPaused() const { return paused_; }

    /// Whether the underlying timer is currently running (for tests/metrics).
    [[nodiscard]] bool isRunning() const;

    /// Number of current subscribers (for tests and diagnostics).
    [[nodiscard]] int subscriberCount() const {
        return static_cast<int>(subscribers_.size());
    }

    /// Frame interval in milliseconds (display cap ~60 FPS).
    static constexpr int kFrameIntervalMs = 16;

private:
    void tick();
    void startIfNeeded();
    void stopIfIdle();

    QTimer* timer_ = nullptr;
    std::vector<AnimationTickable*> subscribers_;
    std::int64_t lastTickMs_ = 0;
    double clockSeconds_ = 0.0;
    bool paused_ = false;
};

}  // namespace darkspark::deck::instruments

#endif  // DARKSPARK_DECK_INSTRUMENTS_ANIMATIONCLOCK_HPP
