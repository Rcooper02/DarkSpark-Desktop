// SPDX-License-Identifier: GPL-3.0-or-later
#include "deck/instruments/InstrumentPreviewPage.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

#include "deck/instruments/CpuInstrument.hpp"
#include "deck/instruments/CpuInstrumentModel.hpp"
#include "deck/instruments/InstrumentSizeMode.hpp"
#include "themes/LegacyTheme.hpp"

namespace darkspark::deck::instruments {

namespace {

/// Build one labelled instrument column: the instrument above a small caption,
/// so the reviewer can tell Small from Large at a glance. The instrument is
/// returned via `outInstrument` so the page can drive it with live data.
QWidget* makeColumn(InstrumentSizeMode mode, const QString& caption,
                    QWidget* parent, CpuInstrument** outInstrument) {
    auto* column = new QWidget(parent);
    auto* col = new QVBoxLayout(column);
    col->setContentsMargins(0, 0, 0, 0);
    col->setSpacing(themes::LegacyTheme::spaceMd());

    auto* instrument = new CpuInstrument(mode, column);
    // No mock values here: the instrument starts empty (all Absent) and is fed a
    // live presentation model by the page's setModel(). The application binds
    // telemetry to the page.
    *outInstrument = instrument;
    // Small is a fixed-size supporting instrument: center it in the column so it
    // reads as a compact quick-glance instrument rather than filling the space.
    // Large keeps its expanding placement unchanged.
    if (mode == InstrumentSizeMode::Small) {
        col->addWidget(instrument, 0, Qt::AlignCenter);
    } else {
        col->addWidget(instrument, 1);
    }

    auto* label = new QLabel(caption, column);
    label->setAlignment(Qt::AlignHCenter);
    label->setStyleSheet(
        QStringLiteral("color: %1; font-size: %2px;")
            .arg(themes::LegacyTheme::textSecondary().name())
            .arg(themes::LegacyTheme::fontAnnotation()));
    col->addWidget(label, 0);

    return column;
}

}  // namespace

InstrumentPreviewPage::InstrumentPreviewPage(Layout layout, QWidget* parent)
    : QWidget(parent) {
    auto* row = new QHBoxLayout(this);
    const int margin = themes::LegacyTheme::space2xl();
    row->setContentsMargins(margin, margin, margin, margin);
    row->setSpacing(themes::LegacyTheme::space3xl());

    const bool showLarge = (layout != Layout::SmallOnly);
    const bool showSmall = (layout != Layout::LargeOnly);

    if (showSmall) {
        CpuInstrument* small = nullptr;
        row->addWidget(makeColumn(InstrumentSizeMode::Small,
                                  QStringLiteral("Small"), this, &small),
                       showLarge ? 2 : 1);
        instruments_.append(small);
    }
    if (showLarge) {
        CpuInstrument* large = nullptr;
        row->addWidget(makeColumn(InstrumentSizeMode::Large,
                                  QStringLiteral("Large"), this, &large),
                       showSmall ? 3 : 1);
        instruments_.append(large);
    }
}

void InstrumentPreviewPage::setModel(const CpuInstrumentModel& model) {
    // Fan the single model out to every hosted instrument so Small and Large
    // always show the same logical values, synchronized.
    for (CpuInstrument* instrument : instruments_) {
        if (instrument != nullptr) {
            instrument->setModel(model);
        }
    }
}

void InstrumentPreviewPage::receiveTelemetry(const models::MetricSample& sample) {
    // Fold the sample into the presentation model. If it changed something this
    // instrument shows, push the updated model to every instrument at once.
    if (adapter_.apply(sample)) {
        setModel(adapter_.model());
    }
}

}  // namespace darkspark::deck::instruments
