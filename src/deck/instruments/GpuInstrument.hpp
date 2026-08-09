// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_DECK_INSTRUMENTS_GPUINSTRUMENT_HPP
#define DARKSPARK_DECK_INSTRUMENTS_GPUINSTRUMENT_HPP

#include <QWidget>

#include "deck/instruments/GpuInstrumentModel.hpp"
#include "deck/instruments/AnimationClock.hpp"
#include "deck/instruments/InstrumentSizeMode.hpp"

class QPaintEvent;
class QShowEvent;
class QHideEvent;

namespace darkspark::deck::instruments {

/// The GPU instrument: the first live subsystem instrument after CPU.
///
/// A DISTINCT class from CpuInstrument with its OWN model (GpuInstrumentModel),
/// its own adapter, its own telemetry, and its own interpolation state -- no
/// CPU-specific assumption leaks into it. It renders through the shared
/// InstrumentRenderer, so the DarkSpark visual LANGUAGE (rings, chamber,
/// conduits, ticks, glow, typography) is not duplicated: CPU, GPU, and every
/// future subsystem share one rendering implementation.
///
/// Milestone 1 identity is intentionally minimal (adjustment: keep GPU visually
/// almost identical to CPU): the "GPU" title and a very subtle accent variation.
/// Full Forge identity (plasma vortex, ambient, particles, heat-reactive
/// background, idle animation, hover/touch, theme system) comes in the dedicated
/// Forge visual milestone -- and because the instrument only builds a neutral
/// render model, that divergence will not touch the pipeline.
///
/// Motion is limited to smooth value interpolation between telemetry updates,
/// matching CPU: dormant except during a transition.
class GpuInstrument : public QWidget, public AnimationTickable {
    Q_OBJECT

public:
    explicit GpuInstrument(InstrumentSizeMode mode, QWidget* parent = nullptr);
    ~GpuInstrument() override;

    void setModel(const GpuInstrumentModel& model);

    /// Wire the shared animation clock. The instrument subscribes/unsubscribes
    /// based on visibility; interpolation and (CPU only) personality motion are
    /// driven by this clock instead of a per-instrument timer.
    void setAnimationClock(AnimationClock* clock);

    // AnimationTickable: advance interpolation (and personality, CPU only) by
    // one frame; report whether continued ticks are needed.
    void advance(double deltaSeconds, double clockSeconds) override;
    [[nodiscard]] bool wantsContinuousAnimation() const override;
    [[nodiscard]] GpuInstrumentModel model() const { return target_; }

    void setSizeMode(InstrumentSizeMode mode);
    [[nodiscard]] InstrumentSizeMode sizeMode() const { return mode_; }

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    void advanceInterpolation();
    [[nodiscard]] bool interpolationSettled() const;
    void applySizePolicyForMode();

    InstrumentSizeMode mode_;
    GpuInstrumentModel target_{};
    GpuInstrumentModel displayed_{};
    // Shared animation clock (owned by Application). Replaces the former
    // per-instrument QTimer. The instrument subscribes while visible and
    // unsubscribes when hidden, so hidden instruments do no animation work.
    AnimationClock* clock_ = nullptr;
    bool subscribed_ = false;
    bool loggedFirstModel_ = false;  ///< diagnostic latch (first setModel)
    bool loggedFirstPaint_ = false;  ///< diagnostic latch (first paintEvent)
};

}  // namespace darkspark::deck::instruments

#endif  // DARKSPARK_DECK_INSTRUMENTS_GPUINSTRUMENT_HPP
