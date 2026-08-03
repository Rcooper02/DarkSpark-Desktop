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
/// so the reviewer can tell Small from Large at a glance.
QWidget* makeColumn(InstrumentSizeMode mode, const QString& caption,
                    QWidget* parent) {
    auto* column = new QWidget(parent);
    auto* col = new QVBoxLayout(column);
    col->setContentsMargins(0, 0, 0, 0);
    col->setSpacing(themes::LegacyTheme::spaceMd());

    auto* instrument = new CpuInstrument(mode, column);
    // Fixed mock values for the visual review: 42% utilization, 61C.
    instrument->setModel(CpuInstrumentModel{42.0, 61.0});
    col->addWidget(instrument, 1);

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
        row->addWidget(
            makeColumn(InstrumentSizeMode::Small, QStringLiteral("Small"), this),
            showLarge ? 2 : 1);
    }
    if (showLarge) {
        row->addWidget(
            makeColumn(InstrumentSizeMode::Large, QStringLiteral("Large"), this),
            showSmall ? 3 : 1);
    }
}

}  // namespace darkspark::deck::instruments
