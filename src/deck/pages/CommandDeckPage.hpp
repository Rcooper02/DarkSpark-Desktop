// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_DECK_PAGES_COMMANDDECKPAGE_HPP
#define DARKSPARK_DECK_PAGES_COMMANDDECKPAGE_HPP

#include <QList>
#include <QWidget>

namespace darkspark::deck::instruments {
class CpuInstrument;
class GpuInstrument;
class MemoryInstrument;
class CoolingInstrument;
}

namespace darkspark::deck::pages {

/// The first real Command Deck composition.
///
/// The page is organized as named REGIONS rather than a flat list of
/// instruments. The regions are not configurable yet -- they are fixed layout
/// containers -- but structuring the page this way now lets the deck evolve
/// toward user-arranged regions without reworking the composition:
///
///   * Status region     -- reserved, visually quiet. A thin strip at the top
///                           for future system status. Empty for now.
///   * Primary region    -- the dominant instrument. Holds the Large CPU
///                           instrument.
///   * Secondary region  -- supporting instruments. Holds five Small subsystem
///                           shells (GPU, Memory, Cooling, Network, Storage) in
///                           a compact 3-over-2 grid. The sixth slot
///                           (bottom-right) is intentionally left empty:
///                           reserved layout capacity for a future subsystem,
///                           rendered as nothing at all -- no placeholder card,
///                           no caption, no centering to disguise it.
///   * Navigation region -- reserved, visually quiet. A thin strip at the bottom
///                           for future page navigation. Empty for now.
///
/// Responsibility boundary: the page COMPOSES instruments and regions and
/// nothing more. It deliberately knows nothing about telemetry -- no
/// MetricSample, no adapter, no providers, no discovery, no subsystem logic --
/// so it never becomes a telemetry coordinator as subsystems are added. It
/// builds the instruments and exposes the primary one; binding live data to
/// that instrument is the composition root's job (Application::startCommandDeck),
/// exactly as the deck and preview routes wire telemetry outside their pages.
class CommandDeckPage : public QWidget {
    Q_OBJECT

public:
    explicit CommandDeckPage(QWidget* parent = nullptr);

    /// The live primary instrument (CPU). Exposed so the composition root can
    /// bind telemetry to it without the page knowing about telemetry types.
    /// Never null after construction.
    [[nodiscard]] instruments::CpuInstrument* primaryInstrument() const {
        return primaryInstrument_;
    }

    /// The live GPU instrument in the secondary region. Exposed for the same
    /// reason as the primary: the composition root binds GPU telemetry to it,
    /// while the page itself stays free of telemetry types. Never null after
    /// construction.
    [[nodiscard]] instruments::GpuInstrument* gpuInstrument() const {
        return gpuInstrument_;
    }

    /// The live Memory instrument. Composition creates it; Application binds it
    /// to memory telemetry, so this page stays telemetry-independent.
    [[nodiscard]] instruments::MemoryInstrument* memoryInstrument() const {
        return memoryInstrument_;
    }

    /// The live Cooling instrument. Composition creates it; Application binds it
    /// to cooling telemetry, so this page stays telemetry-independent.
    [[nodiscard]] instruments::CoolingInstrument* coolingInstrument() const {
        return coolingInstrument_;
    }

private:
    QWidget* buildStatusRegion();
    QWidget* buildNavigationRegion();
    QWidget* buildPrimaryRegion();
    QWidget* buildSecondaryRegion();

    instruments::CpuInstrument* primaryInstrument_ = nullptr;
    instruments::GpuInstrument* gpuInstrument_ = nullptr;
    instruments::MemoryInstrument* memoryInstrument_ = nullptr;
    instruments::CoolingInstrument* coolingInstrument_ = nullptr;
    QList<instruments::CpuInstrument*> shellInstruments_;
};

}  // namespace darkspark::deck::pages

#endif  // DARKSPARK_DECK_PAGES_COMMANDDECKPAGE_HPP
