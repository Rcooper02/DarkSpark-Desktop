// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_DECK_INSTRUMENTS_CPUINSTRUMENT_HPP
#define DARKSPARK_DECK_INSTRUMENTS_CPUINSTRUMENT_HPP

#include <QWidget>

#include "deck/instruments/CpuInstrumentModel.hpp"
#include "deck/instruments/InstrumentSizeMode.hpp"
#include "deck/instruments/InstrumentState.hpp"

class QPaintEvent;

namespace darkspark::deck::instruments {

/// The flagship CPU subsystem instrument.
///
/// This is a single, focused instrument that adapts its composition to an
/// InstrumentSizeMode -- it is NOT a family framework, and there is deliberately
/// no InstrumentBase. The reusable "instrument language" will be extracted only
/// after this flagship is approved on the panel.
///
/// Personality vs. language: the CPU instrument expresses itself through
/// concentric segmented rings. That is the CPU instrument's PERSONALITY, not
/// DarkSpark's identity. Future subsystem instruments must discover their own
/// visual metaphor (cooling as a living fan, network as flow paths, storage as
/// capacity bands) rather than inherit rings.
///
/// Composition philosophy: the instrument owns its square. The empty space is a
/// design element, not leftover room. The dominant utilization value is the one
/// thing read across the room; everything else supports it.
///
/// Motion & interaction: the prototype renders statically in InstrumentState
/// ::Idle, but every element is composed so its eventual motion needs no
/// redesign -- ring fills animate toward values, the center value cross-fades,
/// glow can breathe. The InstrumentState seam is threaded through paint so
/// Hover/Touch/Expanded become state branches later, never a recomposition.
/// Even Idle is intended to carry the faintest sense of presence so the active
/// states have somewhere to grow.
///
/// Theme: all colors come from LegacyTheme. TRON is DarkSpark's first theme, not
/// its permanent identity, so no color is hard-coded and accent choices are
/// resolved through parameters that a future theme/customization layer can drive
/// (monochrome, swapped accents, user-selected or theme-controlled colors).
///
/// Ownership: a QWidget owned by its Qt parent. Threading: GUI thread only.
class CpuInstrument : public QWidget {
    Q_OBJECT

public:
    explicit CpuInstrument(InstrumentSizeMode mode, QWidget* parent = nullptr);

    void setModel(const CpuInstrumentModel& model);
    [[nodiscard]] CpuInstrumentModel model() const { return model_; }

    void setSizeMode(InstrumentSizeMode mode);
    [[nodiscard]] InstrumentSizeMode sizeMode() const { return mode_; }

    /// Reserved interaction seam. The prototype leaves this Idle; setting it
    /// repaints, so future hover/touch handling is a matter of driving this
    /// from enter/leave/touch events without composition changes.
    void setInstrumentState(InstrumentState state);
    [[nodiscard]] InstrumentState instrumentState() const { return state_; }

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    InstrumentSizeMode mode_;
    InstrumentState state_ = InstrumentState::Idle;
    CpuInstrumentModel model_{};
};

}  // namespace darkspark::deck::instruments

#endif  // DARKSPARK_DECK_INSTRUMENTS_CPUINSTRUMENT_HPP
