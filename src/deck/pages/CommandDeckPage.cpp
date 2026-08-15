// SPDX-License-Identifier: GPL-3.0-or-later
#include "deck/pages/CommandDeckPage.hpp"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPen>
#include <QPushButton>
#include <QVBoxLayout>

#include "deck/instruments/CpuInstrument.hpp"
#include "deck/instruments/MemoryInstrument.hpp"
#include "deck/instruments/CoolingInstrument.hpp"
#include "deck/instruments/StorageInstrument.hpp"
#include "deck/instruments/NetworkInstrument.hpp"
#include "deck/instruments/GpuInstrument.hpp"
#include "deck/layout/DeckLayout.hpp"
#include "deck/layout/DeckLayoutEdits.hpp"
#include "deck/layout/InstrumentFactory.hpp"
#include "themes/LegacyTheme.hpp"

namespace darkspark::deck::pages {

using instruments::CpuInstrument;
using instruments::MemoryInstrument;
using instruments::CoolingInstrument;
using instruments::StorageInstrument;
using instruments::NetworkInstrument;
using instruments::GpuInstrument;

namespace {

/// Height of the reserved Status/Navigation strips. Kept thin and quiet: they
/// exist to reserve the region, not to fill space.
constexpr int kReservedStripHeight = 44;

}  // namespace

CommandDeckPage::CommandDeckPage(QWidget* parent)
    : CommandDeckPage(layout::defaultCommandDeckLayout(), parent) {}

CommandDeckPage::CommandDeckPage(const layout::DeckLayout& deckLayout,
                                 QWidget* parent)
    : QWidget(parent), layout_(deckLayout) {
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
    // The bottom strip now hosts the Edit Mode controls: EDIT normally; SAVE and
    // CANCEL while editing. Kept restrained and quiet, consistent with the
    // reserved-strip aesthetic. These are the only affordances added, and they
    // live entirely in the page/edit layer -- no instrument artwork touched.
    auto* region = new QWidget(this);
    region->setObjectName(QStringLiteral("commandDeckNavigationRegion"));
    region->setFixedHeight(kReservedStripHeight);

    auto* row = new QHBoxLayout(region);
    const int m = themes::LegacyTheme::spaceMd();
    row->setContentsMargins(m, 0, m, 0);
    row->setSpacing(themes::LegacyTheme::spaceMd());
    row->addStretch(1);

    editButton_ = new QPushButton(QStringLiteral("EDIT"), region);
    saveButton_ = new QPushButton(QStringLiteral("SAVE"), region);
    cancelButton_ = new QPushButton(QStringLiteral("CANCEL"), region);
    for (QPushButton* b : {editButton_, saveButton_, cancelButton_}) {
        b->setCursor(Qt::PointingHandCursor);
        b->setFocusPolicy(Qt::NoFocus);  // keep key focus on the page
        row->addWidget(b);
    }

    connect(editButton_, &QPushButton::clicked, this,
            &CommandDeckPage::beginEdit);
    connect(saveButton_, &QPushButton::clicked, this, [this] {
        saveEdits();
    });
    connect(cancelButton_, &QPushButton::clicked, this,
            &CommandDeckPage::cancelEdits);

    updateEditControls();
    return region;
}

QWidget* CommandDeckPage::buildPrimaryRegion() {
    // The dominant instrument, built from the layout's Primary-region placement
    // rather than a hard-coded construction. Today that is the Large CPU,
    // centered in its region so it commands its space.
    auto* region = new QWidget(this);
    region->setObjectName(QStringLiteral("commandDeckPrimaryRegion"));
    auto* layout = new QVBoxLayout(region);
    layout->setContentsMargins(0, 0, 0, 0);
    primaryHost_ = region;

    layout->addStretch(1);
    for (const layout::DeckWidgetPlacement& p :
         layout_.placements) {
        if (!p.enabled || p.region != layout::DeckRegion::Primary) {
            continue;
        }
        QWidget* w = layout::createInstrument(p.id, p.sizeMode, region);
        if (w == nullptr) {
            continue;  // unknown/invalid id: fail gracefully, skip it
        }
        captureInstrument(p.id, w);
        registerInstrument(p.id, w);
        layout->addWidget(w, 0, Qt::AlignCenter);
    }
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
    secondaryGrid_ = grid;

    auto* gridHost = new QWidget(region);

    // Placement is data-driven: iterate the layout's Secondary-region entries
    // and place each at its (row, column) with its span. The default layout
    // reproduces the current arrangement exactly -- GPU(0,0) Memory(0,1)
    // Cooling(0,2), Network(1,0) Storage(1,1) -- with cell (1,2) left empty
    // because no placement targets it. The page composes; it does not know
    // telemetry. Instruments are created here but bound to telemetry outside the
    // page (Application), via the typed accessors captured below.
    for (const layout::DeckWidgetPlacement& p :
         layout_.placements) {
        if (!p.enabled || p.region != layout::DeckRegion::Secondary) {
            continue;
        }
        QWidget* w = layout::createInstrument(p.id, p.sizeMode, gridHost);
        if (w == nullptr) {
            continue;  // unknown/invalid id: fail gracefully, skip it
        }
        captureInstrument(p.id, w);
        registerInstrument(p.id, w);
        grid->addWidget(w, p.row, p.column, p.rowSpan, p.columnSpan,
                        Qt::AlignCenter);
    }

    // Keep all three columns and both rows evenly weighted so the empty
    // bottom-right slot holds its place rather than collapsing, and the grid is
    // not stretched to hide it.
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(2, 1);
    grid->setRowStretch(0, 1);
    grid->setRowStretch(1, 1);

    gridHost->setLayout(grid);
    outer->addWidget(gridHost, 0, Qt::AlignCenter);
    outer->addStretch(1);
    return region;
}

void CommandDeckPage::captureInstrument(layout::WidgetId id, QWidget* widget) {
    // Recover the concrete instrument type for the telemetry accessors. The
    // page exposes these typed pointers so Application can bind telemetry
    // without the page knowing telemetry types. Exhaustive over WidgetId, no
    // default, so a new widget id must be handled here.
    switch (id) {
    case layout::WidgetId::Cpu:
        primaryInstrument_ = qobject_cast<CpuInstrument*>(widget);
        break;
    case layout::WidgetId::Gpu:
        gpuInstrument_ = qobject_cast<GpuInstrument*>(widget);
        break;
    case layout::WidgetId::Memory:
        memoryInstrument_ = qobject_cast<MemoryInstrument*>(widget);
        break;
    case layout::WidgetId::Cooling:
        coolingInstrument_ = qobject_cast<CoolingInstrument*>(widget);
        break;
    case layout::WidgetId::Storage:
        storageInstrument_ = qobject_cast<StorageInstrument*>(widget);
        break;
    case layout::WidgetId::Network:
        networkInstrument_ = qobject_cast<NetworkInstrument*>(widget);
        break;
    case layout::WidgetId::Unknown:
        break;
    }
}

void CommandDeckPage::registerInstrument(layout::WidgetId id, QWidget* widget) {
    instrumentById_.insert(static_cast<int>(id), widget);
}

QWidget* CommandDeckPage::widgetForId(layout::WidgetId id) const {
    return instrumentById_.value(static_cast<int>(id), nullptr);
}

void CommandDeckPage::applyLayout(const layout::DeckLayout& layout) {
    // Re-place the EXISTING instruments per `layout`. No factory calls, no
    // deletes: every widget in instrumentById_ is reused, so the pointers
    // Application bound telemetry to stay valid. Widgets not enabled/placed are
    // hidden and detached from their region layout.

    // 1. Detach every known instrument from whichever region layout holds it,
    //    and hide it. Re-adding below restores the enabled ones.
    for (QWidget* w : instrumentById_) {
        if (w == nullptr) {
            continue;
        }
        if (secondaryGrid_ != nullptr) {
            secondaryGrid_->removeWidget(w);
        }
        if (primaryHost_ != nullptr && primaryHost_->layout() != nullptr) {
            primaryHost_->layout()->removeWidget(w);
        }
        w->hide();
    }

    // 2. Place each enabled placement's instrument at its cell and show it.
    for (const layout::DeckWidgetPlacement& p : layout.placements) {
        if (!p.enabled) {
            continue;
        }
        QWidget* w = widgetForId(p.id);
        if (w == nullptr) {
            continue;  // no instrument for this id (e.g. Unknown): skip
        }
        if (p.region == layout::DeckRegion::Primary) {
            if (primaryHost_ != nullptr && primaryHost_->layout() != nullptr) {
                // Primary is a vertical box between two stretches; insert at
                // index 1 so the instrument stays centered.
                auto* box = qobject_cast<QVBoxLayout*>(primaryHost_->layout());
                if (box != nullptr) {
                    box->insertWidget(1, w, 0, Qt::AlignCenter);
                }
            }
        } else {
            if (secondaryGrid_ != nullptr) {
                secondaryGrid_->addWidget(w, p.row, p.column, p.rowSpan,
                                          p.columnSpan, Qt::AlignCenter);
            }
        }
        w->show();
    }

    layout_ = layout;
    update();  // repaint selection overlay against the new arrangement
}

void CommandDeckPage::beginEdit() {
    if (editing_) {
        return;
    }
    editing_ = true;
    preEditLayout_ = layout_;    // exact snapshot for Cancel
    workingLayout_ = layout_;    // mutate this copy
    selected_ = layout::WidgetId::Unknown;
    setFocus(Qt::OtherFocusReason);  // receive arrow keys
    updateEditControls();
    update();
}

bool CommandDeckPage::saveEdits() {
    if (!editing_) {
        return false;
    }
    // Final gate: never commit an invalid layout.
    if (!layout::isValidLayout(workingLayout_)) {
        return false;  // stay in edit mode; nothing committed, nothing emitted
    }
    applyLayout(workingLayout_);
    editing_ = false;
    selected_ = layout::WidgetId::Unknown;
    updateEditControls();
    update();
    emit layoutCommitted(layout_);  // composition root persists it
    return true;
}

void CommandDeckPage::cancelEdits() {
    if (!editing_) {
        return;
    }
    // Restore the EXACT pre-edit arrangement; never persists.
    applyLayout(preEditLayout_);
    editing_ = false;
    selected_ = layout::WidgetId::Unknown;
    workingLayout_ = layout::DeckLayout{};
    updateEditControls();
    update();
}

bool CommandDeckPage::moveSelection(layout::DeckRegion region, int row,
                                    int column) {
    if (!editing_ || selected_ == layout::WidgetId::Unknown) {
        return false;
    }
    if (!layout::moveWidget(workingLayout_, selected_, region, row, column)) {
        return false;  // invalid/overlapping: rejected, working copy unchanged
    }
    applyLayout(workingLayout_);  // live preview, still not persisted
    return true;
}

bool CommandDeckPage::toggleSelectedEnabled() {
    if (!editing_ || selected_ == layout::WidgetId::Unknown) {
        return false;
    }
    const layout::DeckWidgetPlacement* p =
        layout::findPlacement(workingLayout_, selected_);
    if (p == nullptr) {
        return false;
    }
    const bool ok =
        layout::setWidgetEnabled(workingLayout_, selected_, !p->enabled);
    if (ok) {
        applyLayout(workingLayout_);
    }
    return ok;
}

void CommandDeckPage::updateEditControls() {
    if (editButton_ != nullptr) {
        editButton_->setVisible(!editing_);
    }
    if (saveButton_ != nullptr) {
        saveButton_->setVisible(editing_);
    }
    if (cancelButton_ != nullptr) {
        cancelButton_->setVisible(editing_);
    }
}

void CommandDeckPage::paintEvent(QPaintEvent* event) {
    QWidget::paintEvent(event);
    if (!editing_ || selected_ == layout::WidgetId::Unknown) {
        return;
    }
    QWidget* w = widgetForId(selected_);
    if (w == nullptr || !w->isVisible()) {
        return;
    }
    // Restrained selection outline: a thin cyan rounded rect just outside the
    // selected instrument, drawn in the PAGE layer (never in InstrumentRenderer
    // or instrument artwork). Maps the widget's rect into page coordinates.
    const QRect r = QRect(w->mapTo(this, QPoint(0, 0)), w->size())
                        .adjusted(-3, -3, 3, 3);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    QPen pen(themes::LegacyTheme::accentCyan());
    pen.setWidth(2);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(r, 6, 6);
}

void CommandDeckPage::mousePressEvent(QMouseEvent* event) {
    if (!editing_) {
        QWidget::mousePressEvent(event);
        return;
    }
    // Select the instrument under the cursor, if any.
    const QPoint pagePos = event->pos();
    for (auto it = instrumentById_.constBegin();
         it != instrumentById_.constEnd(); ++it) {
        QWidget* w = it.value();
        if (w == nullptr || !w->isVisible()) {
            continue;
        }
        const QRect r(w->mapTo(this, QPoint(0, 0)), w->size());
        if (r.contains(pagePos)) {
            selected_ = static_cast<layout::WidgetId>(it.key());
            update();
            return;
        }
    }
    // Clicked empty space: clear selection.
    selected_ = layout::WidgetId::Unknown;
    update();
}

void CommandDeckPage::keyPressEvent(QKeyEvent* event) {
    if (!editing_) {
        QWidget::keyPressEvent(event);
        return;
    }
    // Keyboard supplements the buttons: arrows move the selection by one cell,
    // Space toggles enabled, Enter saves, Escape cancels.
    const layout::DeckWidgetPlacement* p =
        selected_ == layout::WidgetId::Unknown
            ? nullptr
            : layout::findPlacement(workingLayout_, selected_);
    switch (event->key()) {
    case Qt::Key_Left:
        if (p != nullptr) {
            moveSelection(p->region, p->row, p->column - 1);
        }
        return;
    case Qt::Key_Right:
        if (p != nullptr) {
            moveSelection(p->region, p->row, p->column + 1);
        }
        return;
    case Qt::Key_Up:
        if (p != nullptr) {
            moveSelection(p->region, p->row - 1, p->column);
        }
        return;
    case Qt::Key_Down:
        if (p != nullptr) {
            moveSelection(p->region, p->row + 1, p->column);
        }
        return;
    case Qt::Key_Space:
        toggleSelectedEnabled();
        return;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        saveEdits();
        return;
    case Qt::Key_Escape:
        cancelEdits();
        return;
    default:
        QWidget::keyPressEvent(event);
        return;
    }
}

}  // namespace darkspark::deck::pages
