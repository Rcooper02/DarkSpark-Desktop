// SPDX-License-Identifier: GPL-3.0-or-later
#include "deck/pages/CommandDeckPage.hpp"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include "deck/instruments/CpuInstrument.hpp"
#include "deck/instruments/GpuInstrument.hpp"
#include "deck/instruments/InstrumentSizeMode.hpp"
#include "themes/LegacyTheme.hpp"

namespace darkspark::deck::pages {

using instruments::CpuInstrument;
using instruments::GpuInstrument;
using instruments::InstrumentSizeMode;

namespace {

/// Height of the reserved Status/Navigation strips. Kept thin and quiet: they
/// exist to reserve the region, not to fill space.
constexpr int kReservedStripHeight = 44;

}  // namespace

CommandDeckPage::CommandDeckPage(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Top: Status region (reserved, quiet).
    root->addWidget(buildStatusRegion(), 0);

    // Middle: the content band holding Primary + Secondary side by side.
    auto* content = new QWidget(this);
    auto* contentRow = new QHBoxLayout(content);
    const int margin = themes::LegacyTheme::space2xl();
    contentRow->setContentsMargins(margin, margin, margin, margin);
    contentRow->setSpacing(themes::LegacyTheme::space3xl());
    // Primary gets less stretch than Secondary so the Large instrument keeps a
    // focused column on the left while the grid uses the wider right area.
    contentRow->addWidget(buildPrimaryRegion(), 2);
    contentRow->addWidget(buildSecondaryRegion(), 3);
    root->addWidget(content, 1);

    // Bottom: Navigation region (reserved, quiet).
    root->addWidget(buildNavigationRegion(), 0);
}

QWidget* CommandDeckPage::buildStatusRegion() {
    // Reserved: a thin, quiet strip at the top. Intentionally empty for now --
    // a future home for system status. It reserves the region so the deck's
    // vertical composition already accounts for it.
    auto* region = new QWidget(this);
    region->setObjectName(QStringLiteral("commandDeckStatusRegion"));
    region->setFixedHeight(kReservedStripHeight);
    return region;
}

QWidget* CommandDeckPage::buildNavigationRegion() {
    // Reserved: a thin, quiet strip at the bottom. Intentionally empty for now
    // -- a future home for page navigation.
    auto* region = new QWidget(this);
    region->setObjectName(QStringLiteral("commandDeckNavigationRegion"));
    region->setFixedHeight(kReservedStripHeight);
    return region;
}

QWidget* CommandDeckPage::buildPrimaryRegion() {
    // The dominant instrument: the live Large CPU instrument, centered in its
    // region so it commands its space.
    auto* region = new QWidget(this);
    region->setObjectName(QStringLiteral("commandDeckPrimaryRegion"));
    auto* layout = new QVBoxLayout(region);
    layout->setContentsMargins(0, 0, 0, 0);

    primaryInstrument_ = new CpuInstrument(InstrumentSizeMode::Large, region);
    // Title defaults to "CPU"; this is the real, live instrument (not a shell).
    layout->addStretch(1);
    layout->addWidget(primaryInstrument_, 0, Qt::AlignCenter);
    layout->addStretch(1);
    return region;
}

QWidget* CommandDeckPage::buildSecondaryRegion() {
    // Five Small subsystem shells in a compact 3-over-2 grid. The sixth slot
    // (row 1, column 2) is intentionally left empty: reserved layout capacity
    // for a future subsystem instrument. Nothing is placed there -- no
    // placeholder, no caption -- and the second row is NOT centered: Network and
    // Storage stay left-aligned under GPU and Memory, so the empty bottom-right
    // slot holds its position as visible future capacity.
    auto* region = new QWidget(this);
    region->setObjectName(QStringLiteral("commandDeckSecondaryRegion"));

    auto* outer = new QVBoxLayout(region);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addStretch(1);

    auto* grid = new QGridLayout();
    grid->setHorizontalSpacing(themes::LegacyTheme::space3xl());
    grid->setVerticalSpacing(themes::LegacyTheme::spaceXl());

    // Explicit (row, col) placement. Row 0: GPU(0,0) Memory(0,1) Cooling(0,2).
    // Row 1: Network(1,0) Storage(1,1). Slot (1,2) is never populated.
    //
    // The GPU slot now hosts the live GpuInstrument (its own class); the other
    // four remain temporary CpuInstrument shells awaiting their real subsystem
    // instruments. The live GPU is created here (composition) but bound to
    // telemetry outside the page (Application), so the page stays
    // telemetry-independent.
    gpuInstrument_ = new GpuInstrument(InstrumentSizeMode::Small, region);
    grid->addWidget(gpuInstrument_, 0, 0, Qt::AlignCenter);

    // Remaining shells, placed after the GPU slot: Memory, Cooling, Network,
    // Storage. GPU is intentionally skipped here since it is now live.
    struct ShellPlacement {
        const char* title;
        int row;
        int col;
    };
    static const ShellPlacement kShells[] = {
        {"Memory", 0, 1}, {"Cooling", 0, 2}, {"Network", 1, 0},
        {"Storage", 1, 1}};
    for (const ShellPlacement& s : kShells) {
        auto* shell = new CpuInstrument(InstrumentSizeMode::Small, region);
        shell->setTitle(QString::fromUtf8(s.title));
        // A shell: dormant conduits (all-Absent model) plus the "Awaiting
        // Telemetry" caption. No telemetry is ever bound to these. Each Small
        // shell is centered within its own grid cell (the validated maximum-cap
        // sizing keeps it at its intended footprint).
        shell->setAwaitingTelemetry(true);
        shellInstruments_.append(shell);
        grid->addWidget(shell, s.row, s.col, Qt::AlignCenter);
    }
    // Keep all three columns and both rows evenly weighted so the empty
    // bottom-right slot holds its place rather than collapsing, and the grid is
    // not stretched to hide it.
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(2, 1);
    grid->setRowStretch(0, 1);
    grid->setRowStretch(1, 1);

    auto* gridHost = new QWidget(region);
    gridHost->setLayout(grid);
    outer->addWidget(gridHost, 0, Qt::AlignCenter);
    outer->addStretch(1);
    return region;
}

}  // namespace darkspark::deck::pages
