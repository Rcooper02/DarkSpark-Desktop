// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_DECK_INSTRUMENTS_NETWORKINSTRUMENT_HPP
#define DARKSPARK_DECK_INSTRUMENTS_NETWORKINSTRUMENT_HPP

#include <QWidget>

#include "deck/instruments/AnimationClock.hpp"
#include "deck/instruments/InstrumentSizeMode.hpp"
#include "deck/instruments/NetworkInstrumentModel.hpp"

class QPaintEvent;
class QShowEvent;
class QHideEvent;

namespace darkspark::deck::instruments {

/// The Network instrument: the sixth live subsystem instrument, at Command Deck
/// slot (1, 0).
///
/// Functionality only -- no personality, animation, or detail view. A distinct
/// class with its own model, adapter, telemetry, and interpolation, rendering
/// through the shared InstrumentRenderer.
///
/// V1 face: primary = download throughput, secondary = upload throughput, both
/// with adaptive unit formatting (B/s, KB/s, MB/s, GB/s by magnitude) done in
/// this instrument only -- the renderer never interprets network units. Link
/// state, cumulative bytes, and interface identity are retained in the model but
/// not shown on this face. Single-ring in V1.
class NetworkInstrument : public QWidget, public AnimationTickable {
    Q_OBJECT

public:
    explicit NetworkInstrument(InstrumentSizeMode mode, QWidget* parent = nullptr);
    ~NetworkInstrument() override;

    void setModel(const NetworkInstrumentModel& model);

    /// Wire the shared animation clock. The instrument subscribes/unsubscribes
    /// based on visibility; interpolation and (CPU only) personality motion are
    /// driven by this clock instead of a per-instrument timer.
    void setAnimationClock(AnimationClock* clock);

    // AnimationTickable: advance interpolation (and personality, CPU only) by
    // one frame; report whether continued ticks are needed.
    void advance(double deltaSeconds, double clockSeconds) override;
    [[nodiscard]] bool wantsContinuousAnimation() const override;
    [[nodiscard]] NetworkInstrumentModel model() const { return target_; }

    void setSizeMode(InstrumentSizeMode mode);
    [[nodiscard]] InstrumentSizeMode sizeMode() const { return mode_; }

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

    /// Adaptive throughput formatting: returns the numeric text and, via
    /// `suffixOut`, the matching unit ("B/s", "KB/s", "MB/s", "GB/s"). Static for
    /// direct unit testing. Binary steps (1024).
    [[nodiscard]] static QString formatRate(double bytesPerSec,
                                            QString& suffixOut);

protected:
    void paintEvent(QPaintEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    void advanceInterpolation();
    [[nodiscard]] bool interpolationSettled() const;
    void applySizePolicyForMode();

    InstrumentSizeMode mode_;
    NetworkInstrumentModel target_{};
    NetworkInstrumentModel displayed_{};
    // Shared animation clock (owned by Application). Replaces the former
    // per-instrument QTimer. The instrument subscribes while visible and
    // unsubscribes when hidden, so hidden instruments do no animation work.
    AnimationClock* clock_ = nullptr;
    bool subscribed_ = false;
};

}  // namespace darkspark::deck::instruments

#endif  // DARKSPARK_DECK_INSTRUMENTS_NETWORKINSTRUMENT_HPP
