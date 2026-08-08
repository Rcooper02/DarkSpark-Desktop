// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_DECK_INSTRUMENTS_NETWORKINSTRUMENT_HPP
#define DARKSPARK_DECK_INSTRUMENTS_NETWORKINSTRUMENT_HPP

#include <QWidget>

#include "deck/instruments/InstrumentSizeMode.hpp"
#include "deck/instruments/NetworkInstrumentModel.hpp"

class QPaintEvent;
class QTimer;

namespace darkspark::deck::instruments {

/// The Network instrument: the sixth live subsystem instrument, at Command Deck
/// slot (1, 0).
///
/// Functionality only -- no personality, animation, or detail view. A distinct
/// class with its own model, adapter, telemetry, and interpolation, rendering
/// through the shared InstrumentRenderer.
///
/// V1 face: primary = download throughput, secondary = upload throughput, both
/// with adaptive unit formatting (B/s, KB/s, MB/s, GB/s by magnitude) done in
/// this instrument only -- the renderer never interprets network units. Link
/// state, cumulative bytes, and interface identity are retained in the model but
/// not shown on this face. Single-ring in V1.
class NetworkInstrument : public QWidget {
    Q_OBJECT

public:
    explicit NetworkInstrument(InstrumentSizeMode mode, QWidget* parent = nullptr);
    ~NetworkInstrument() override;

    void setModel(const NetworkInstrumentModel& model);
    [[nodiscard]] NetworkInstrumentModel model() const { return target_; }

    void setSizeMode(InstrumentSizeMode mode);
    [[nodiscard]] InstrumentSizeMode sizeMode() const { return mode_; }

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

    /// Adaptive throughput formatting: returns the numeric text and, via
    /// `suffixOut`, the matching unit ("B/s", "KB/s", "MB/s", "GB/s"). Static for
    /// direct unit testing. Binary steps (1024).
    [[nodiscard]] static QString formatRate(double bytesPerSec,
                                            QString& suffixOut);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void advanceInterpolation();
    [[nodiscard]] bool interpolationSettled() const;
    void applySizePolicyForMode();

    InstrumentSizeMode mode_;
    NetworkInstrumentModel target_{};
    NetworkInstrumentModel displayed_{};
    QTimer* transitionTimer_;
};

}  // namespace darkspark::deck::instruments

#endif  // DARKSPARK_DECK_INSTRUMENTS_NETWORKINSTRUMENT_HPP
