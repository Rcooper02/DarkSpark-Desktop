// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_DECK_INSTRUMENTS_INSTRUMENTPERSONALITY_HPP
#define DARKSPARK_DECK_INSTRUMENTS_INSTRUMENTPERSONALITY_HPP

namespace darkspark::deck::instruments {

/// Static, per-instrument personality CONFIGURATION -- pure data, no state, no
/// Qt, no telemetry. This is the reusable-platform core: a personality is
/// entirely described by configuration + mapping, never by renderer-specific
/// logic, so future instrument personalities and themes are added by supplying a
/// different InstrumentPersonality (and, if needed, a different mapping curve)
/// WITHOUT touching InstrumentRenderer.
///
/// All coefficients are dimensionless and bounded; they parameterise the
/// continuous mappings in PersonalityMapping. Defaults describe a calm, neutral
/// identity; CPU ("The Core") supplies its own instance.
struct InstrumentPersonality {
    /// Ambient energy floor at idle (0 = inert, calm-but-alive baseline) and the
    /// ceiling approached under full load. ambient in [floor, ceiling].
    double ambientIdle = 0.15;
    double ambientLoad = 1.0;

    /// Idle "breathing": a faint always-present oscillation so the instrument
    /// feels alive at rest. Amplitude is a small fraction of ambient; period is
    /// in seconds. Kept subtle by design.
    double breathingAmplitude = 0.06;
    double breathingPeriodSeconds = 4.0;

    /// Electrical flow speed (phase advance per second, radians) at idle and at
    /// full load; load interpolates between them.
    double flowSpeedIdle = 0.6;
    double flowSpeedLoad = 3.0;

    /// Pulse/reactor presence amplitude at full load (0 at idle). Drives the
    /// centre's stronger presence under load.
    double pulseLoadAmplitude = 0.5;
    double pulsePeriodSeconds = 1.2;

    /// Thermal influence: how strongly temperature warms the visual character,
    /// in [0, 1], applied across the soft temperature range below.
    double thermalInfluence = 0.6;
    double thermalLowC = 40.0;   ///< at/below this, no warming
    double thermalHighC = 90.0;  ///< at/above this, full configured warming

    /// Temporal smoothing factor for ambient/warmth easing per tick-second
    /// (higher = snappier). Keeps mappings continuous, never a threshold jump.
    double smoothingPerSecond = 4.0;

    /// Whether idle breathing runs at all. When false the instrument is fully
    /// static once interpolation settles (used by the dormant/neutral default).
    bool idleBreathing = false;

    /// A calm, neutral default identity (no warming, no breathing) that -- when
    /// combined with neutral render params -- reproduces the pre-personality
    /// look. Instruments opt into a richer identity explicitly.
    [[nodiscard]] static InstrumentPersonality neutral() {
        InstrumentPersonality p;
        p.ambientIdle = 0.0;
        p.ambientLoad = 0.0;
        p.breathingAmplitude = 0.0;
        p.flowSpeedIdle = 0.0;
        p.flowSpeedLoad = 0.0;
        p.pulseLoadAmplitude = 0.0;
        p.thermalInfluence = 0.0;
        p.idleBreathing = false;
        return p;
    }

    /// CPU "The Core": reactor-like, calm-but-alive at idle, increasingly
    /// energetic under load, warming with temperature. Values tuned to be subtle;
    /// final feel is Fedora/XENEON-validated.
    [[nodiscard]] static InstrumentPersonality core() {
        InstrumentPersonality p;
        // Artistic tuning: the reactor PULSE leads, motion carries the
        // personality, glow stays restrained. Values chosen to feel alive and
        // "expensive" without being distracting; final feel is XENEON-validated.
        p.ambientIdle = 0.22;         // calm-but-alive floor
        p.ambientLoad = 0.85;         // energetic, not blown out
        p.breathingAmplitude = 0.08;  // subtle idle life
        p.breathingPeriodSeconds = 4.0;
        p.flowSpeedIdle = 0.8;        // gentle drift at rest
        p.flowSpeedLoad = 4.5;        // lively travelling flow under load
        p.pulseLoadAmplitude = 0.8;   // the LEAD: strong reactor pulse
        p.pulsePeriodSeconds = 1.1;
        p.thermalInfluence = 0.85;    // clear cyan->amber hue shift when hot
        p.thermalLowC = 40.0;
        p.thermalHighC = 88.0;
        p.smoothingPerSecond = 4.0;
        p.idleBreathing = true;
        return p;
    }
};

/// The neutral render parameters the renderer consumes -- subsystem-agnostic
/// VISUALIZATION state, never telemetry. The renderer reads these plain scalars
/// and never knows which instrument or personality produced them. At neutral
/// defaults (ambientIntensity 0, warmth 0, no pulse) the renderer reproduces its
/// pre-personality output exactly.
struct PersonalityRenderParams {
    double ambientIntensity = 0.0;  ///< [0,1] overall energy level for this frame
    double warmth = 0.0;            ///< [0,1] chromatic warm-shift of the accents
    double flowPhase = 0.0;         ///< radians; directional/electrical movement
    double pulse = 0.0;             ///< [0,1] centre/reactor presence this frame
};

/// Evolving personality RUNTIME state, separated from config. Advanced only by
/// the animation clock (never by telemetry). Deterministic given a sequence of
/// (delta, inputs).
struct PersonalityState {
    double flowPhase = 0.0;        ///< accumulated electrical-flow phase (radians)
    double pulsePhase = 0.0;       ///< accumulated pulse phase (radians)
    double breathePhase = 0.0;     ///< accumulated breathing phase (radians)
    double smoothedAmbient = 0.0;  ///< eased ambient level
    double smoothedWarmth = 0.0;   ///< eased warmth level
    bool primed = false;           ///< smoothed values jumped to first target yet
};

}  // namespace darkspark::deck::instruments

#endif  // DARKSPARK_DECK_INSTRUMENTS_INSTRUMENTPERSONALITY_HPP
