// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_DECK_INSTRUMENTS_MEMORYINSTRUMENT_HPP
#define DARKSPARK_DECK_INSTRUMENTS_MEMORYINSTRUMENT_HPP

#include <QWidget>

#include "deck/instruments/InstrumentSizeMode.hpp"
#include "deck/instruments/MemoryInstrumentModel.hpp"

class QPaintEvent;
class QTimer;

namespace darkspark::deck::instruments {

/// The Memory instrument ("The Library"): the third live subsystem instrument
/// after CPU and GPU.
///
/// A DISTINCT class with its OWN model (MemoryInstrumentModel), its own adapter,
/// its own telemetry, and its own interpolation state -- no CPU/GPU-specific
/// assumption leaks into it. It renders through the shared InstrumentRenderer,
/// so the DarkSpark visual language is not duplicated.
///
/// This milestone is FUNCTIONALITY ONLY: the "Memory" title, the utilization
/// percentage as the primary value, and a "used / total GB" secondary line.
/// Library personality (any bespoke visual identity, reactive background,
/// flowing effects, idle animation, themes, interaction) is intentionally NOT
/// implemented here and comes in a later dedicated milestone. Because the
/// instrument only builds a neutral render model, that later divergence will not
/// touch the pipeline.
///
/// Motion is limited to smooth value interpolation between telemetry updates,
/// matching CPU and GPU: dormant except during a transition.
class MemoryInstrument : public QWidget {
    Q_OBJECT

public:
    explicit MemoryInstrument(InstrumentSizeMode mode, QWidget* parent = nullptr);
    ~MemoryInstrument() override;

    void setModel(const MemoryInstrumentModel& model);
    [[nodiscard]] MemoryInstrumentModel model() const { return target_; }

    void setSizeMode(InstrumentSizeMode mode);
    [[nodiscard]] InstrumentSizeMode sizeMode() const { return mode_; }

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

    /// Format a byte count as a compact GB string (e.g. "12.4"). Exposed as a
    /// static helper so it can be unit-tested without constructing a widget.
    /// Uses binary GB (GiB, / 1024^3), the convention Linux memory tools use.
    [[nodiscard]] static QString formatGigabytes(double bytes);

    /// Build the secondary "used / total GB" line from a model, or an empty
    /// string when the byte figures are not both available (so the renderer
    /// shows its neutral placeholder). Static for direct unit testing.
    [[nodiscard]] static QString formatSecondaryLine(
        const MemoryInstrumentModel& model);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void advanceInterpolation();
    [[nodiscard]] bool interpolationSettled() const;
    void applySizePolicyForMode();

    InstrumentSizeMode mode_;
    MemoryInstrumentModel target_{};
    MemoryInstrumentModel displayed_{};
    QTimer* transitionTimer_;
};

}  // namespace darkspark::deck::instruments

#endif  // DARKSPARK_DECK_INSTRUMENTS_MEMORYINSTRUMENT_HPP
