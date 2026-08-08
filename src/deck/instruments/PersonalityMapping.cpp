// SPDX-License-Identifier: GPL-3.0-or-later
#include "deck/instruments/PersonalityMapping.hpp"

#include <cmath>

namespace darkspark::deck::instruments::personality {

namespace {
constexpr double kTwoPi = 6.283185307179586;

// Lerp between a and b by t in [0,1].
constexpr double lerp(double a, double b, double t) { return a + (b - a) * t; }

// Advance a phase by (radiansPerSecond * dt), wrapped into [0, 2pi) so it never
// grows unbounded.
double advancePhase(double phase, double radPerSec, double dt) {
    double next = phase + radPerSec * dt;
    if (next >= kTwoPi) {
        next = std::fmod(next, kTwoPi);
    } else if (next < 0.0) {
        next = std::fmod(next, kTwoPi);
        if (next < 0.0) {
            next += kTwoPi;
        }
    }
    return next;
}
}  // namespace

double targetAmbient(const InstrumentPersonality& p,
                     const PersonalityInputs& in) {
    if (!in.activityAvailable) {
        return p.ambientIdle;  // no fabricated motion when activity is unknown
    }
    const double a = clamp01(in.activity);
    return lerp(p.ambientIdle, p.ambientLoad, smoothstep(a));
}

double targetWarmth(const InstrumentPersonality& p,
                    const PersonalityInputs& in) {
    if (!in.temperatureAvailable || p.thermalInfluence <= 0.0) {
        return 0.0;
    }
    const double span = p.thermalHighC - p.thermalLowC;
    if (span <= 0.0) {
        return 0.0;
    }
    const double t = (in.temperatureC - p.thermalLowC) / span;
    return clamp01(smoothstep(t)) * clamp01(p.thermalInfluence);
}

PersonalityRenderParams advance(const InstrumentPersonality& p,
                                const PersonalityInputs& in, double deltaSeconds,
                                PersonalityState& state) {
    double dt = deltaSeconds;
    if (dt < 0.0) {
        dt = 0.0;
    }

    const double ambientTarget = targetAmbient(p, in);
    const double warmthTarget = targetWarmth(p, in);

    if (!state.primed) {
        // First evaluation jumps to the targets so nothing eases up from zero on
        // the very first frame; subsequent frames ease.
        state.smoothedAmbient = ambientTarget;
        state.smoothedWarmth = warmthTarget;
        state.primed = true;
    } else {
        // Exponential easing toward the target, framerate-independent: the
        // fraction applied scales with dt so pacing is stable across frame times.
        const double k = clamp01(p.smoothingPerSecond * dt);
        state.smoothedAmbient += (ambientTarget - state.smoothedAmbient) * k;
        state.smoothedWarmth += (warmthTarget - state.smoothedWarmth) * k;
    }

    const double activity =
        in.activityAvailable ? clamp01(in.activity) : 0.0;

    // Flow speed scales with activity; breathing and pulse advance at their own
    // periods. Phases are wrapped and never unbounded.
    const double flowSpeed = lerp(p.flowSpeedIdle, p.flowSpeedLoad, activity);
    state.flowPhase = advancePhase(state.flowPhase, flowSpeed, dt);

    const double breatheSpeed =
        p.breathingPeriodSeconds > 0.0 ? kTwoPi / p.breathingPeriodSeconds : 0.0;
    state.breathePhase = advancePhase(state.breathePhase, breatheSpeed, dt);

    const double pulseSpeed =
        p.pulsePeriodSeconds > 0.0 ? kTwoPi / p.pulsePeriodSeconds : 0.0;
    state.pulsePhase = advancePhase(state.pulsePhase, pulseSpeed, dt);

    PersonalityRenderParams out;

    // Ambient intensity = smoothed ambient plus a faint breathing oscillation
    // (only when idle breathing is enabled), clamped to [0,1].
    double ambient = state.smoothedAmbient;
    if (p.idleBreathing && p.breathingAmplitude > 0.0) {
        ambient += std::sin(state.breathePhase) * p.breathingAmplitude;
    }
    out.ambientIntensity = clamp01(ambient);

    out.warmth = clamp01(state.smoothedWarmth);
    out.flowPhase = state.flowPhase;

    // Pulse presence grows with activity; a half-rectified sine keeps it a
    // positive [0,1] "presence" rather than a bipolar wobble.
    const double pulseAmp = p.pulseLoadAmplitude * activity;
    const double pulseWave = 0.5 * (std::sin(state.pulsePhase) + 1.0);
    out.pulse = clamp01(pulseAmp * pulseWave);

    return out;
}

bool hasIdleMotion(const InstrumentPersonality& p) {
    return p.idleBreathing && p.breathingAmplitude > 0.0
           && p.breathingPeriodSeconds > 0.0;
}

}  // namespace darkspark::deck::instruments::personality
