// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_DECK_INSTRUMENTS_RINGNORMALIZATIONPOLICY_HPP
#define DARKSPARK_DECK_INSTRUMENTS_RINGNORMALIZATIONPOLICY_HPP

#include <algorithm>
#include <memory>

namespace darkspark::deck::instruments {

/// How an instrument turns a raw value into a conduit-fill fraction [0, 1].
///
/// This is a PRESENTATION policy owned by the instrument, never by the service
/// or the renderer. It produces VISUALIZATION state (RenderValue::progress), not
/// telemetry: the numeric value an instrument shows is always the true reading;
/// the ring fill is a separate, interchangeable choice. From the renderer's
/// perspective a policy is completely stateless -- the renderer only ever sees a
/// final [0, 1] number and never knows how it was produced.
///
/// V1 ships AdaptiveObservedMaxPolicy as Cooling's default. Named future options
/// (HardwareMaximumPolicy, FixedRangePolicy, NoNormalizationPolicy) can replace
/// it per-instrument WITHOUT touching the renderer or telemetry -- only which
/// policy the instrument constructs.
class RingNormalizationPolicy {
public:
    virtual ~RingNormalizationPolicy() = default;

    /// Map the latest raw value to a [0, 1] fill. May be stateful across calls
    /// (e.g. an adaptive observed maximum); the instrument owns that state.
    [[nodiscard]] virtual double normalize(double value) = 0;
};

/// Cooling's V1 default: an adaptive, presentation-only activity indicator.
///
/// It tracks a smoothed rolling maximum of observed values and fills the ring as
/// value / observedMax. This is EXPLICITLY NOT a hardware capacity or a "% of
/// maximum cooling": no trustworthy maximum RPM exists, so the ring conveys
/// relative activity on THIS machine only. The true RPM is always shown
/// numerically; only the ring is relative.
///
/// Behaviour:
///   - observedMax rises immediately to any new peak (so a real spike shows),
///   - and decays slowly toward the current value with a floor, so a single
///     transient peak does not permanently flatten the ring.
/// Deterministic given an input sequence, so it is unit-testable without Qt.
class AdaptiveObservedMaxPolicy : public RingNormalizationPolicy {
public:
    /// `floor` keeps early/low readings from pegging the ring before a range is
    /// observed; `decay` in (0,1) is the per-update pull of observedMax toward
    /// the latest value (smaller = slower decay).
    explicit AdaptiveObservedMaxPolicy(double floor = 800.0,
                                       double decay = 0.02)
        : floor_(floor), decay_(decay), observedMax_(floor) {}

    [[nodiscard]] double normalize(double value) override {
        const double v = std::max(0.0, value);
        if (v > observedMax_) {
            observedMax_ = v;  // rise immediately to a new peak
        } else {
            // Decay observedMax slowly toward v, but never below the floor, so
            // the ring stays meaningful and a lone spike relaxes over time.
            const double target = std::max(floor_, v);
            observedMax_ += (target - observedMax_) * decay_;
        }
        if (observedMax_ <= 0.0) {
            return 0.0;
        }
        return std::clamp(v / observedMax_, 0.0, 1.0);
    }

    [[nodiscard]] double observedMaxForTest() const { return observedMax_; }

private:
    double floor_;
    double decay_;
    double observedMax_;
};

}  // namespace darkspark::deck::instruments

#endif  // DARKSPARK_DECK_INSTRUMENTS_RINGNORMALIZATIONPOLICY_HPP
