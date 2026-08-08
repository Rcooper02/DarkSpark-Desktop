// SPDX-License-Identifier: GPL-3.0-or-later
//
// Deterministic tests for the personality mapping platform. Qt-free.
//
// Covers the required cases: neutral reproduces neutral render params; CPU
// mappings monotonic and clamped; temperature warmth continuous; idle breathing
// bounded; unavailable telemetry decays safely; phase advancement and wrapping;
// determinism; and that mapping never depends on anything but its inputs.

#include <cmath>
#include <cstdio>

#include "deck/instruments/InstrumentPersonality.hpp"
#include "deck/instruments/PersonalityMapping.hpp"

using namespace darkspark::deck::instruments;
using namespace darkspark::deck::instruments::personality;

namespace {
int g_failures = 0;
void reportFail(const char* e, const char* f, int l) {
    std::fprintf(stderr, "FAIL: %s  (%s:%d)\n", e, f, l);
    ++g_failures;
}
#define CHECK(c) do { if (!(c)) reportFail(#c, __FILE__, __LINE__); } while (0)

constexpr double kTwoPi = 6.283185307179586;

// Neutral personality yields all-zero render params for ANY input -> the
// renderer reproduces its pre-personality output exactly.
void test_neutral_reproduces_neutral() {
    const auto neut = InstrumentPersonality::neutral();
    CHECK(!hasIdleMotion(neut));
    for (double u = 0.0; u <= 1.0; u += 0.1) {
        for (double t = 0.0; t <= 100.0; t += 20.0) {
            PersonalityState st;
            const PersonalityInputs in{u, true, t, true};
            const auto r = advance(neut, in, 0.016, st);
            CHECK(r.ambientIntensity == 0.0);
            CHECK(r.warmth == 0.0);
            CHECK(r.pulse == 0.0);
            CHECK(r.flowPhase == 0.0);
        }
    }
}

// Ambient target is monotonic non-decreasing in activity and clamped.
void test_ambient_monotonic_clamped() {
    const auto core = InstrumentPersonality::core();
    double prev = -1.0;
    for (double u = 0.0; u <= 1.0; u += 0.05) {
        const PersonalityInputs in{u, true, 40.0, true};
        const double a = targetAmbient(core, in);
        CHECK(a >= prev - 1e-9);  // non-decreasing
        CHECK(a >= 0.0 && a <= 1.0);
        prev = a;
    }
}

// Warmth is continuous and monotonic across the thermal range, clamped, and
// zero below the low threshold.
void test_warmth_continuous_monotonic() {
    const auto core = InstrumentPersonality::core();
    const PersonalityInputs cold{0.5, true, core.thermalLowC - 5.0, true};
    CHECK(targetWarmth(core, cold) == 0.0);
    double prev = -1.0;
    double lastStep = 0.0;
    for (double t = core.thermalLowC; t <= core.thermalHighC; t += 1.0) {
        const PersonalityInputs in{0.5, true, t, true};
        const double w = targetWarmth(core, in);
        CHECK(w >= prev - 1e-9);       // monotonic
        CHECK(w >= 0.0 && w <= 1.0);   // clamped
        if (prev >= 0.0) {
            const double step = std::fabs(w - prev);
            CHECK(step < 0.1);         // continuous: no large jumps
            (void)lastStep;
            lastStep = step;
        }
        prev = w;
    }
    // Above the high threshold it saturates (does not exceed the influence cap).
    const PersonalityInputs hot{0.5, true, core.thermalHighC + 50.0, true};
    CHECK(targetWarmth(core, hot) <= 1.0);
    CHECK(targetWarmth(core, hot) <= core.thermalInfluence + 1e-9);
}

// Idle breathing keeps ambient bounded within [0,1] and oscillating, never
// exploding, over a long run at idle.
void test_idle_breathing_bounded() {
    const auto core = InstrumentPersonality::core();
    PersonalityState st;
    const PersonalityInputs idle{0.0, true, 40.0, true};
    double minA = 2.0;
    double maxA = -1.0;
    for (int i = 0; i < 2000; ++i) {
        const auto r = advance(core, idle, 0.016, st);
        CHECK(r.ambientIntensity >= 0.0 && r.ambientIntensity <= 1.0);
        minA = std::min(minA, r.ambientIntensity);
        maxA = std::max(maxA, r.ambientIntensity);
    }
    // There is some motion (breathing) but it stays subtle and bounded.
    CHECK(maxA > minA);            // it breathes
    CHECK(maxA <= 1.0 && minA >= 0.0);
}

// Unavailable telemetry decays safely: ambient eases to the idle floor and
// warmth to zero, with no fabricated motion.
void test_unavailable_decays_safely() {
    const auto core = InstrumentPersonality::core();
    PersonalityState st;
    // Prime hot + busy.
    const PersonalityInputs busy{1.0, true, 85.0, true};
    for (int i = 0; i < 50; ++i) {
        (void)advance(core, busy, 0.016, st);
    }
    CHECK(st.smoothedWarmth > 0.0);
    // Now telemetry goes unavailable; warmth must decay toward 0, ambient toward
    // the idle floor.
    const PersonalityInputs gone{0.0, false, 0.0, false};
    for (int i = 0; i < 400; ++i) {
        (void)advance(core, gone, 0.016, st);
    }
    CHECK(st.smoothedWarmth < 1e-3);
    CHECK(std::fabs(st.smoothedAmbient - core.ambientIdle) < 1e-2);
}

// Phase advances and wraps into [0, 2pi); flow speed scales with activity.
void test_phase_advance_and_wrap() {
    const auto core = InstrumentPersonality::core();
    PersonalityState st;
    const PersonalityInputs busy{1.0, true, 40.0, true};
    for (int i = 0; i < 10000; ++i) {
        const auto r = advance(core, busy, 0.016, st);
        CHECK(r.flowPhase >= 0.0 && r.flowPhase < kTwoPi);
    }
    // Flow advances faster under load than at idle over the same time.
    PersonalityState slow;
    PersonalityState fast;
    const PersonalityInputs idle{0.0, true, 40.0, true};
    for (int i = 0; i < 10; ++i) {
        (void)advance(core, idle, 0.016, slow);
        (void)advance(core, busy, 0.016, fast);
    }
    CHECK(fast.flowPhase > slow.flowPhase);
}

// Determinism: identical inputs give identical state evolution.
void test_determinism() {
    const auto core = InstrumentPersonality::core();
    PersonalityState a;
    PersonalityState b;
    const PersonalityInputs in{0.4, true, 55.0, true};
    for (int i = 0; i < 500; ++i) {
        const auto ra = advance(core, in, 0.016, a);
        const auto rb = advance(core, in, 0.016, b);
        CHECK(ra.ambientIntensity == rb.ambientIntensity);
        CHECK(ra.warmth == rb.warmth);
        CHECK(ra.flowPhase == rb.flowPhase);
        CHECK(ra.pulse == rb.pulse);
    }
}

// Pulse grows with activity and stays in [0,1]; zero at idle.
void test_pulse_scales_with_activity() {
    const auto core = InstrumentPersonality::core();
    // At idle, pulse amplitude is zero regardless of phase.
    PersonalityState idleState;
    const PersonalityInputs idle{0.0, true, 40.0, true};
    for (int i = 0; i < 200; ++i) {
        const auto r = advance(core, idle, 0.016, idleState);
        CHECK(r.pulse == 0.0);
    }
    // Under load, pulse becomes positive at some point and stays bounded.
    PersonalityState loadState;
    const PersonalityInputs load{1.0, true, 40.0, true};
    double maxPulse = 0.0;
    for (int i = 0; i < 400; ++i) {
        const auto r = advance(core, load, 0.016, loadState);
        CHECK(r.pulse >= 0.0 && r.pulse <= 1.0);
        maxPulse = std::max(maxPulse, r.pulse);
    }
    CHECK(maxPulse > 0.0);
}

// A negative delta is treated as no time passing (defensive; clock clamps too).
void test_negative_delta_safe() {
    const auto core = InstrumentPersonality::core();
    PersonalityState st;
    const PersonalityInputs in{0.5, true, 60.0, true};
    (void)advance(core, in, 0.016, st);
    const double phaseBefore = st.flowPhase;
    const auto r = advance(core, in, -1.0, st);
    CHECK(st.flowPhase == phaseBefore);  // no advance on negative delta
    CHECK(r.ambientIntensity >= 0.0 && r.ambientIntensity <= 1.0);
}

}  // namespace

int main() {
    test_neutral_reproduces_neutral();
    test_ambient_monotonic_clamped();
    test_warmth_continuous_monotonic();
    test_idle_breathing_bounded();
    test_unavailable_decays_safely();
    test_phase_advance_and_wrap();
    test_determinism();
    test_pulse_scales_with_activity();
    test_negative_delta_safe();
    if (g_failures == 0) {
        std::puts("All PersonalityMapping tests passed.");
        return 0;
    }
    std::fprintf(stderr, "%d personality mapping check(s) failed.\n", g_failures);
    return 1;
}
