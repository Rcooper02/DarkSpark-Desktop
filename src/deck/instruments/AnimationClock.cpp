// SPDX-License-Identifier: GPL-3.0-or-later
#include "deck/instruments/AnimationClock.hpp"

#include <QDateTime>
#include <QLoggingCategory>
#include <QTimer>

#include <algorithm>

namespace darkspark::deck::instruments {

namespace {
// Runtime diagnostic: how many AnimationClock instances exist. The Command Deck
// must create exactly one. Logged under darkspark.anim so Fedora can PROVE the
// single-timer design at runtime (QT_LOGGING_RULES="darkspark.anim=true"),
// rather than inferring it from code inspection.
Q_LOGGING_CATEGORY(lcAnim, "darkspark.anim")
int g_activeClocks = 0;
}  // namespace

AnimationClock::AnimationClock(QObject* parent) : QObject(parent) {
    ++g_activeClocks;
    qCInfo(lcAnim).noquote()
        << "AnimationClock constructed; active clock instances =" << g_activeClocks
        << "(Command Deck expects exactly 1)";
    timer_ = new QTimer(this);
    timer_->setInterval(kFrameIntervalMs);
    timer_->setTimerType(Qt::PreciseTimer);
    connect(timer_, &QTimer::timeout, this, [this]() { tick(); });
}

AnimationClock::~AnimationClock() {
    --g_activeClocks;
}

void AnimationClock::subscribe(AnimationTickable* tickable) {
    if (tickable == nullptr) {
        return;
    }
    if (std::find(subscribers_.begin(), subscribers_.end(), tickable)
        == subscribers_.end()) {
        subscribers_.push_back(tickable);
    }
    startIfNeeded();
}

void AnimationClock::unsubscribe(AnimationTickable* tickable) {
    subscribers_.erase(
        std::remove(subscribers_.begin(), subscribers_.end(), tickable),
        subscribers_.end());
    stopIfIdle();
}

void AnimationClock::requestAnimation() { startIfNeeded(); }

void AnimationClock::setPaused(bool paused) {
    if (paused_ == paused) {
        return;
    }
    paused_ = paused;
    if (paused_) {
        timer_->stop();
    } else {
        startIfNeeded();
    }
}

bool AnimationClock::isRunning() const { return timer_->isActive(); }

void AnimationClock::startIfNeeded() {
    if (paused_ || timer_->isActive()) {
        return;
    }
    bool anyWants = false;
    for (const AnimationTickable* t : subscribers_) {
        if (t != nullptr && t->wantsContinuousAnimation()) {
            anyWants = true;
            break;
        }
    }
    if (!anyWants) {
        return;
    }
    lastTickMs_ = QDateTime::currentMSecsSinceEpoch();
    timer_->start();
    qCDebug(lcAnim).noquote()
        << "clock START; subscribers =" << static_cast<int>(subscribers_.size())
        << "intervalMs =" << kFrameIntervalMs;
}

void AnimationClock::stopIfIdle() {
    for (const AnimationTickable* t : subscribers_) {
        if (t != nullptr && t->wantsContinuousAnimation()) {
            return;  // still someone to animate
        }
    }
    timer_->stop();
    qCDebug(lcAnim).noquote()
        << "clock STOP (no subscriber wants animation); subscribers ="
        << static_cast<int>(subscribers_.size());
}

void AnimationClock::tick() {
    const std::int64_t nowMs = QDateTime::currentMSecsSinceEpoch();
    double delta = static_cast<double>(nowMs - lastTickMs_) / 1000.0;
    lastTickMs_ = nowMs;
    // Guard against a pathological delta (clock jump, long stall): clamp so a
    // resumed clock never applies a huge jump to interpolation/phase.
    if (delta < 0.0) {
        delta = 0.0;
    }
    constexpr double kMaxDelta = 0.1;  // 100 ms
    if (delta > kMaxDelta) {
        delta = kMaxDelta;
    }
    clockSeconds_ += delta;

    // Copy the pointers we tick so a subscriber that unsubscribes during its own
    // advance() cannot invalidate the iteration.
    const std::vector<AnimationTickable*> snapshot = subscribers_;
    for (AnimationTickable* t : snapshot) {
        if (t != nullptr && t->wantsContinuousAnimation()) {
            t->advance(delta, clockSeconds_);
        }
    }
    stopIfIdle();
}

}  // namespace darkspark::deck::instruments
