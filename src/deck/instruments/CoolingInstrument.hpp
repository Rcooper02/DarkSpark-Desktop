// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_DECK_INSTRUMENTS_COOLINGINSTRUMENT_HPP
#define DARKSPARK_DECK_INSTRUMENTS_COOLINGINSTRUMENT_HPP

#include <QWidget>

#include <memory>

#include "deck/instruments/CoolingInstrumentModel.hpp"
#include "deck/instruments/InstrumentSizeMode.hpp"
#include "deck/instruments/RingNormalizationPolicy.hpp"

class QPaintEvent;
class QTimer;

namespace darkspark::deck::instruments {

/// The Cooling instrument: the fourth live subsystem instrument.
///
/// Functionality only -- no Cryo Core personality, no fan/airflow animation, no
/// thermal-warning visuals. A distinct class with its own model, adapter,
/// telemetry, and interpolation, rendering through the shared InstrumentRenderer.
///
/// The primary is RAW RPM, never a fabricated percentage: the numeric value is
/// the true RPM ("1180") with a " RPM" suffix. The outer ring is a
/// presentation-only activity fill produced by an interchangeable
/// RingNormalizationPolicy (default: AdaptiveObservedMaxPolicy) -- it does NOT
/// imply "% of maximum cooling". The secondary is a coolant temperature or a
/// second fan's RPM when present. Single-ring in V1 (hasSecondaryRing false).
class CoolingInstrument : public QWidget {
    Q_OBJECT

public:
    explicit CoolingInstrument(InstrumentSizeMode mode, QWidget* parent = nullptr);
    ~CoolingInstrument() override;

    void setModel(const CoolingInstrumentModel& model);
    [[nodiscard]] CoolingInstrumentModel model() const { return target_; }

    void setSizeMode(InstrumentSizeMode mode);
    [[nodiscard]] InstrumentSizeMode sizeMode() const { return mode_; }

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

    /// Build the secondary line: a coolant temperature value ("34") when a
    /// coolant temp is present, else a second fan's RPM value ("900"), else
    /// empty. The unit suffix is chosen separately by the caller. Static for
    /// direct unit testing.
    [[nodiscard]] static QString formatSecondaryText(
        const CoolingInstrumentModel& model);
    /// The suffix that pairs with formatSecondaryText for a given model.
    [[nodiscard]] static QString secondarySuffix(
        const CoolingInstrumentModel& model);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void advanceInterpolation();
    [[nodiscard]] bool interpolationSettled() const;
    void applySizePolicyForMode();

    InstrumentSizeMode mode_;
    CoolingInstrumentModel target_{};
    CoolingInstrumentModel displayed_{};
    QTimer* transitionTimer_;
    std::unique_ptr<RingNormalizationPolicy> ringPolicy_;
};

}  // namespace darkspark::deck::instruments

#endif  // DARKSPARK_DECK_INSTRUMENTS_COOLINGINSTRUMENT_HPP
