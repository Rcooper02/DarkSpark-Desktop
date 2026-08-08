// SPDX-License-Identifier: GPL-3.0-or-later
//
// Deterministic tests for AnimationClock. Links Qt6::Core: the clock owns a real
// QTimer, so we pump a QCoreApplication event loop briefly to let ticks fire.
//
// Proves: subscription/unsubscription and subscriber count; a SINGLE clock ticks
// all subscribers (one driver); unsubscribed/hidden clients do not advance;
// pause halts ticking; and the clock stops entirely when no subscriber wants
// animation.

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QTimer>

#include <cstdio>
#include <cstdlib>

#include "deck/instruments/AnimationClock.hpp"

using namespace darkspark::deck::instruments;

namespace {
int g_failures = 0;
void reportFail(const char* e, const char* f, int l) {
    std::fprintf(stderr, "FAIL: %s  (%s:%d)\n", e, f, l);
    ++g_failures;
}
#define CHECK(c) do { if (!(c)) reportFail(#c, __FILE__, __LINE__); } while (0)

// A mock tickable that counts advances and can declare whether it wants
// animation (mimicking "interpolating" vs "settled").
class MockTickable : public AnimationTickable {
public:
    void advance(double /*delta*/, double /*clock*/) override { ++advances; }
    [[nodiscard]] bool wantsContinuousAnimation() const override { return wants; }
    int advances = 0;
    bool wants = true;
};

// Pump the event loop for approximately `ms` milliseconds so timer ticks fire.
void pump(int ms) {
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < ms) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
    }
}

void test_subscription_count() {
    AnimationClock clock;
    MockTickable a;
    MockTickable b;
    CHECK(clock.subscriberCount() == 0);
    clock.subscribe(&a);
    clock.subscribe(&b);
    CHECK(clock.subscriberCount() == 2);
    clock.subscribe(&a);  // idempotent
    CHECK(clock.subscriberCount() == 2);
    clock.unsubscribe(&a);
    CHECK(clock.subscriberCount() == 1);
    clock.unsubscribe(&b);
    CHECK(clock.subscriberCount() == 0);
}

void test_one_driver_ticks_all() {
    AnimationClock clock;
    MockTickable a;
    MockTickable b;
    MockTickable c;
    a.wants = b.wants = c.wants = true;
    clock.subscribe(&a);
    clock.subscribe(&b);
    clock.subscribe(&c);
    CHECK(clock.isRunning());
    pump(80);  // ~5 frames at 16ms
    // A single clock advanced all three subscribers a similar number of times.
    CHECK(a.advances > 0);
    CHECK(b.advances > 0);
    CHECK(c.advances > 0);
    // They are driven by the SAME clock, so counts are within one tick of each.
    CHECK(std::abs(a.advances - b.advances) <= 1);
    CHECK(std::abs(b.advances - c.advances) <= 1);
}

void test_unsubscribed_does_not_advance() {
    AnimationClock clock;
    MockTickable stays;
    MockTickable leaves;
    stays.wants = leaves.wants = true;
    clock.subscribe(&stays);
    clock.subscribe(&leaves);
    pump(48);
    clock.unsubscribe(&leaves);
    const int leavesAt = leaves.advances;
    pump(48);
    CHECK(leaves.advances == leavesAt);  // no advance after unsubscribe
    CHECK(stays.advances > leavesAt);    // the remaining one kept going
}

void test_settled_client_not_ticked_and_clock_stops() {
    AnimationClock clock;
    MockTickable settled;
    settled.wants = false;  // already settled: wants no animation
    clock.subscribe(&settled);
    // No subscriber wants animation -> the clock must not be running.
    CHECK(!clock.isRunning());
    pump(48);
    CHECK(settled.advances == 0);
}

void test_stops_when_all_settle() {
    AnimationClock clock;
    MockTickable m;
    m.wants = true;
    clock.subscribe(&m);
    CHECK(clock.isRunning());
    pump(48);
    CHECK(m.advances > 0);
    // Now it "settles": stops wanting animation. After the next tick the clock
    // should detect no one wants animation and stop.
    m.wants = false;
    pump(64);
    CHECK(!clock.isRunning());
    const int settledAt = m.advances;
    pump(48);
    CHECK(m.advances == settledAt);  // fully stopped, no more advances
}

void test_pause_halts_and_resume() {
    AnimationClock clock;
    MockTickable m;
    m.wants = true;
    clock.subscribe(&m);
    CHECK(clock.isRunning());
    clock.setPaused(true);
    CHECK(!clock.isRunning());
    const int at = m.advances;
    pump(48);
    CHECK(m.advances == at);  // paused: no advances
    clock.setPaused(false);
    CHECK(clock.isRunning());
    pump(48);
    CHECK(m.advances > at);  // resumed
}

void test_requestAnimation_restarts() {
    AnimationClock clock;
    MockTickable m;
    m.wants = false;  // settled
    clock.subscribe(&m);
    CHECK(!clock.isRunning());
    // New "telemetry" arrives: it now wants animation and requests a restart.
    m.wants = true;
    clock.requestAnimation();
    CHECK(clock.isRunning());
    pump(48);
    CHECK(m.advances > 0);
}

}  // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    test_subscription_count();
    test_one_driver_ticks_all();
    test_unsubscribed_does_not_advance();
    test_settled_client_not_ticked_and_clock_stops();
    test_stops_when_all_settle();
    test_pause_halts_and_resume();
    test_requestAnimation_restarts();
    if (g_failures == 0) {
        std::puts("All AnimationClock tests passed.");
        return 0;
    }
    std::fprintf(stderr, "%d animation clock check(s) failed.\n", g_failures);
    return 1;
}
