// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_DECK_INSTRUMENTS_PERSONALITYMAPPING_HPP
#define DARKSPARK_DECK_INSTRUMENTS_PERSONALITYMAPPING_HPP

#include "deck/instruments/InstrumentPersonality.hpp"

namespace darkspark::deck::instruments::personality {

/// The telemetry inputs a personality reacts to. Deliberately minimal and
/// subsystem-neutral: a normalized activity level and an optional temperature,
/// each with an availability flag. Instruments translate their own model into
/// this (CPU: utilization%/100 and package temp). Personality NEVER reads raw
/// MetricSamples or mutates telemetry -- these are copies of already-displayed
/// values.
struct PersonalityInputs {
    double activity = 0.0;       ///< [0,1] normalized activity (e.g. util/100)
    bool activityAvailable = false;
    double temperatureC = 0.0;   ///< degrees Celsius
    bool temperatureAvailable = false;
};

/// clamp helper (kept local so this stays dependency-light and testable).
[[nodiscard]] constexpr double clamp01(double v) {
    return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v);
}

/// Smoothstep in [0,1]; continuous, no threshold jumps.
[[nodiscard]] constexpr double smoothstep(double x) {
    const double t = clamp01(x);
    return t * t * (3.0 - 2.0 * t);
}

/// Target ambient level for the given activity, on [ambientIdle, ambientLoad],
/// eased by smoothstep so it rises continuously with load. When activity is
/// unavailable, the target is the idle floor (no fabricated motion).
[[nodiscard]] double targetAmbient(const InstrumentPersonality& p,
                                   const PersonalityInputs& in);

/// Target warmth [0,1] from temperature across [thermalLowC, thermalHighC],
/// scaled by thermalInfluence. 0 when temperature is unavailable.
[[nodiscard]] double targetWarmth(const InstrumentPersonality& p,
                                  const PersonalityInputs& in);

/// Advance runtime state by `deltaSeconds` given current inputs, then return the
/// render params for this frame. Pure and deterministic: same (state, inputs,
/// delta) always yields the same next state + params. Eases smoothed ambient and
/// warmth toward their targets (never a jump after priming), advances flow/pulse/
/// breathing phases by load-scaled speeds, and composes the final bounded params.
///
/// The returned params are always within valid ranges; the runtime `state` is
/// updated in place.
[[nodiscard]] PersonalityRenderParams advance(const InstrumentPersonality& p,
                                              const PersonalityInputs& in,
                                              double deltaSeconds,
                                              PersonalityState& state);

/// Whether, given personality + inputs, the instrument has live idle motion that
/// should keep the shared clock running even when interpolation has settled
/// (i.e. idle breathing is enabled and there is something to animate). Neutral
/// personalities return false so the deck goes fully static when idle.
[[nodiscard]] bool hasIdleMotion(const InstrumentPersonality& p);

}  // namespace darkspark::deck::instruments::personality

#endif  // DARKSPARK_DECK_INSTRUMENTS_PERSONALITYMAPPING_HPP
