// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_DECK_INSTRUMENTS_INSTRUMENTPREVIEWPAGE_HPP
#define DARKSPARK_DECK_INSTRUMENTS_INSTRUMENTPREVIEWPAGE_HPP

#include <QWidget>

namespace darkspark::deck::instruments {

/// Isolated review surface for the CpuInstrument prototype.
///
/// This is intentionally simple -- not a preview framework. It hosts the CPU
/// instrument at one or more size modes so the flagship composition can be
/// judged on the panel. Reachable only via the --instrument-preview launch
/// flag, so a normal launch never sees it and the stable dashboard is untouched.
///
/// The internal Layout enum lets the review switch between comparing sizes and
/// judging one size full-panel, without any settings UI.
class InstrumentPreviewPage : public QWidget {
    Q_OBJECT

public:
    /// Which instruments the preview shows. SideBySide is the default because
    /// comparing Small and Large is part of judging whether they read as one
    /// family.
    enum class Layout { SideBySide, LargeOnly, SmallOnly };

    explicit InstrumentPreviewPage(Layout layout = Layout::SideBySide,
                                   QWidget* parent = nullptr);
};

}  // namespace darkspark::deck::instruments

#endif  // DARKSPARK_DECK_INSTRUMENTS_INSTRUMENTPREVIEWPAGE_HPP
