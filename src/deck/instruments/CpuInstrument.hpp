// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_DECK_INSTRUMENTS_CPUINSTRUMENT_HPP
#define DARKSPARK_DECK_INSTRUMENTS_CPUINSTRUMENT_HPP

#include <QString>
#include <QWidget>

#include "deck/instruments/CpuInstrumentModel.hpp"
#include "deck/instruments/AnimationClock.hpp"
#include "deck/instruments/InstrumentPersonality.hpp"
#include "deck/instruments/InstrumentSizeMode.hpp"
#include "deck/instruments/InstrumentState.hpp"

class QPaintEvent;
class QShowEvent;
class QHideEvent;

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
class CpuInstrument : public QWidget, public AnimationTickable {
    Q_OBJECT

public:
    explicit CpuInstrument(InstrumentSizeMode mode, QWidget* parent = nullptr);

    void setModel(const CpuInstrumentModel& model);

    /// Wire the shared animation clock. The instrument subscribes/unsubscribes
    /// based on visibility; interpolation and (CPU only) personality motion are
    /// driven by this clock instead of a per-instrument timer.
    void setAnimationClock(AnimationClock* clock);

    // AnimationTickable: advance interpolation (and personality, CPU only) by
    // one frame; report whether continued ticks are needed.
    void advance(double deltaSeconds, double clockSeconds) override;
    [[nodiscard]] bool wantsContinuousAnimation() const override;

    /// Enable a personality identity (default is neutral). CPU uses
    /// InstrumentPersonality::core() to become "The Core".
    void setPersonality(const InstrumentPersonality& personality);
    [[nodiscard]] CpuInstrumentModel model() const { return target_; }

    /// The instrument's title. Defaults to "CPU" so the primary CPU instrument
    /// renders exactly as before. Temporary subsystem shells (GPU, Memory, ...)
    /// override this while awaiting their real instrument. Presentation only:
    /// it changes the drawn label and nothing else.
    void setTitle(const QString& title);
    [[nodiscard]] QString title() const { return title_; }

    /// Mark this instrument as a temporary shell awaiting telemetry. A shell
    /// renders its dormant conduits (from an all-Absent model) and a restrained
    /// "Awaiting Telemetry" caption in place of the numeric secondary line. This
    /// is how the Command Deck shows a subsystem slot as present-but-not-yet-live
    /// without any subsystem-specific class or geometry. Defaults to false, so
    /// the real CPU instrument is never a shell.
    void setAwaitingTelemetry(bool awaiting);
    [[nodiscard]] bool isAwaitingTelemetry() const { return awaitingTelemetry_; }

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
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    void advanceInterpolation();
    [[nodiscard]] bool interpolationSettled() const;
    void applySizePolicyForMode();

    InstrumentSizeMode mode_;
    InstrumentState state_ = InstrumentState::Idle;
    QString title_ = QStringLiteral("CPU");
    bool awaitingTelemetry_ = false;

    /// The instrument interpolates what it DRAWS toward the latest model it was
    /// GIVEN, so telemetry updates read as smooth transitions rather than jumps.
    /// target_ is the most recent model from setModel(); displayed_ is what is
    /// currently painted and eases toward target_. This is the only motion in
    /// the instrument: it runs only while a transition is in progress and stops
    /// when displayed_ reaches target_ (no idle animation).
    CpuInstrumentModel target_{};
    CpuInstrumentModel displayed_{};
    // Shared animation clock (owned by Application). Replaces the former
    // per-instrument QTimer. The instrument subscribes while visible and
    // unsubscribes when hidden, so hidden instruments do no animation work.
    AnimationClock* clock_ = nullptr;
    bool subscribed_ = false;

    // Personality: static config + evolving runtime state. Neutral by default so
    // the instrument reproduces its pre-personality look until core() is enabled.
    // Advanced by the shared clock, never by telemetry.
    InstrumentPersonality personality_ = InstrumentPersonality::neutral();
    PersonalityState personalityState_{};
    PersonalityRenderParams personalityParams_{};
};

}  // namespace darkspark::deck::instruments

#endif  // DARKSPARK_DECK_INSTRUMENTS_CPUINSTRUMENT_HPP
