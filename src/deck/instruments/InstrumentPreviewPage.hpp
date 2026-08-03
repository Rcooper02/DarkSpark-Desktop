// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_DECK_INSTRUMENTS_INSTRUMENTPREVIEWPAGE_HPP
#define DARKSPARK_DECK_INSTRUMENTS_INSTRUMENTPREVIEWPAGE_HPP

#include <QList>
#include <QWidget>

#include "deck/instruments/CpuInstrumentModel.hpp"
#include "deck/instruments/CpuInstrumentModelAdapter.hpp"
#include "models/MetricSample.hpp"

namespace darkspark::deck::instruments {

class CpuInstrument;

/// Isolated review surface for the CpuInstrument prototype.
///
/// This is intentionally simple -- not a preview framework. It hosts the CPU
/// instrument at one or more size modes so the flagship composition can be
/// judged on the panel. Reachable only via the --instrument-preview launch
/// flag, so a normal launch never sees it and the stable dashboard is untouched.
///
/// The internal Layout enum lets the review switch between comparing sizes and
/// judging one size full-panel, without any settings UI.
///
/// The page owns no data of its own: it is fed a CpuInstrumentModel from
/// outside (the application binds live telemetry to it) and fans that single
/// model out to every hosted instrument, so Small and Large always show the
/// same logical values.
class InstrumentPreviewPage : public QWidget {
    Q_OBJECT

public:
    /// Which instruments the preview shows. SideBySide is the default because
    /// comparing Small and Large is part of judging whether they read as one
    /// family.
    enum class Layout { SideBySide, LargeOnly, SmallOnly };

    explicit InstrumentPreviewPage(Layout layout = Layout::SideBySide,
                                   QWidget* parent = nullptr);

    /// Apply a presentation model to every hosted instrument, keeping the sizes
    /// visually synchronized.
    void setModel(const CpuInstrumentModel& model);

public slots:
    /// Receive one telemetry sample, fold it into the presentation model via the
    /// adapter, and update the instruments. This mirrors how the deck window
    /// receives telemetry, so the application wires providers to the preview the
    /// same way it wires them to the dashboard -- the preview stays isolated but
    /// uses the real pipeline.
    void receiveTelemetry(const models::MetricSample& sample);

private:
    QList<CpuInstrument*> instruments_;
    CpuInstrumentModelAdapter adapter_;
};

}  // namespace darkspark::deck::instruments

#endif  // DARKSPARK_DECK_INSTRUMENTS_INSTRUMENTPREVIEWPAGE_HPP
