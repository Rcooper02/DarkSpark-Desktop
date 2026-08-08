// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_DECK_INSTRUMENTS_STORAGEINSTRUMENT_HPP
#define DARKSPARK_DECK_INSTRUMENTS_STORAGEINSTRUMENT_HPP

#include <QWidget>

#include "deck/instruments/AnimationClock.hpp"
#include "deck/instruments/InstrumentSizeMode.hpp"
#include "deck/instruments/StorageInstrumentModel.hpp"

class QPaintEvent;
class QShowEvent;
class QHideEvent;

namespace darkspark::deck::instruments {

/// The Storage instrument: the fifth live subsystem instrument.
///
/// Functionality only -- no personality, animation, or detail view. A distinct
/// class with its own model, adapter, telemetry, and interpolation, rendering
/// through the shared InstrumentRenderer.
///
/// V1 face: primary = filesystem utilization ("73" + "%"), outer ring filled
/// from the 0-100 range; secondary = "used / total" capacity ("453 / 620" +
/// " GB"). Temperature and throughput are retained in the model (for later
/// detail views) but not shown on this face. Single-ring in V1.
class StorageInstrument : public QWidget, public AnimationTickable {
    Q_OBJECT

public:
    explicit StorageInstrument(InstrumentSizeMode mode, QWidget* parent = nullptr);
    ~StorageInstrument() override;

    void setModel(const StorageInstrumentModel& model);

    /// Wire the shared animation clock. The instrument subscribes/unsubscribes
    /// based on visibility; interpolation and (CPU only) personality motion are
    /// driven by this clock instead of a per-instrument timer.
    void setAnimationClock(AnimationClock* clock);

    // AnimationTickable: advance interpolation (and personality, CPU only) by
    // one frame; report whether continued ticks are needed.
    void advance(double deltaSeconds, double clockSeconds) override;
    [[nodiscard]] bool wantsContinuousAnimation() const override;
    [[nodiscard]] StorageInstrumentModel model() const { return target_; }

    void setSizeMode(InstrumentSizeMode mode);
    [[nodiscard]] InstrumentSizeMode sizeMode() const { return mode_; }

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

    /// Format a byte count as a compact binary-GB (GiB) string, e.g. "453".
    [[nodiscard]] static QString formatGigabytes(double bytes);
    /// The "used / total" value line (no unit), or empty when both bytes are not
    /// available. The " GB" suffix is applied by the caller.
    [[nodiscard]] static QString formatSecondaryLine(
        const StorageInstrumentModel& model);

protected:
    void paintEvent(QPaintEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    void advanceInterpolation();
    [[nodiscard]] bool interpolationSettled() const;
    void applySizePolicyForMode();

    InstrumentSizeMode mode_;
    StorageInstrumentModel target_{};
    StorageInstrumentModel displayed_{};
    // Shared animation clock (owned by Application). Replaces the former
    // per-instrument QTimer. The instrument subscribes while visible and
    // unsubscribes when hidden, so hidden instruments do no animation work.
    AnimationClock* clock_ = nullptr;
    bool subscribed_ = false;
};

}  // namespace darkspark::deck::instruments

#endif  // DARKSPARK_DECK_INSTRUMENTS_STORAGEINSTRUMENT_HPP
