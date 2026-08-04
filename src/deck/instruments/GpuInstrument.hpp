// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_DECK_INSTRUMENTS_GPUINSTRUMENT_HPP
#define DARKSPARK_DECK_INSTRUMENTS_GPUINSTRUMENT_HPP

#include <QWidget>

#include "deck/instruments/GpuInstrumentModel.hpp"
#include "deck/instruments/InstrumentSizeMode.hpp"

class QPaintEvent;
class QTimer;

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
class GpuInstrument : public QWidget {
    Q_OBJECT

public:
    explicit GpuInstrument(InstrumentSizeMode mode, QWidget* parent = nullptr);
    ~GpuInstrument() override;

    void setModel(const GpuInstrumentModel& model);
    [[nodiscard]] GpuInstrumentModel model() const { return target_; }

    void setSizeMode(InstrumentSizeMode mode);
    [[nodiscard]] InstrumentSizeMode sizeMode() const { return mode_; }

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void advanceInterpolation();
    [[nodiscard]] bool interpolationSettled() const;
    void applySizePolicyForMode();

    InstrumentSizeMode mode_;
    GpuInstrumentModel target_{};
    GpuInstrumentModel displayed_{};
    QTimer* transitionTimer_;
};

}  // namespace darkspark::deck::instruments

#endif  // DARKSPARK_DECK_INSTRUMENTS_GPUINSTRUMENT_HPP
