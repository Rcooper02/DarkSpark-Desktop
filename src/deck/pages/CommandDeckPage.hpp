// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_DECK_PAGES_COMMANDDECKPAGE_HPP
#define DARKSPARK_DECK_PAGES_COMMANDDECKPAGE_HPP

#include <QHash>
#include <QList>
#include <QRect>
#include <QWidget>

#include "deck/layout/DeckLayout.hpp"

class QGridLayout;
class QVBoxLayout;
class QPushButton;
class QPaintEvent;
class QMouseEvent;
class QKeyEvent;
class QResizeEvent;
class QPainter;

namespace darkspark::deck::instruments {
class CpuInstrument;
class GpuInstrument;
class MemoryInstrument;
class CoolingInstrument;
class StorageInstrument;
class NetworkInstrument;
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

    /// Construct from a resolved layout (e.g. loaded from persistence). The page
    /// stores and builds from this layout and knows nothing about how it was
    /// resolved -- no JSON, no files. The parent-only constructor above delegates
    /// here with defaultCommandDeckLayout() for compatibility.
    explicit CommandDeckPage(const layout::DeckLayout& deckLayout,
                             QWidget* parent = nullptr);

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

    /// The live Storage instrument. Composition creates it; Application binds it
    /// to storage telemetry, so this page stays telemetry-independent.
    [[nodiscard]] instruments::StorageInstrument* storageInstrument() const {
        return storageInstrument_;
    }

    /// The live Network instrument. Composition creates it; Application binds it
    /// to network telemetry, so this page stays telemetry-independent.
    [[nodiscard]] instruments::NetworkInstrument* networkInstrument() const {
        return networkInstrument_;
    }

    // --- Edit Mode -----------------------------------------------------------
    // Edit Mode lets the user rearrange THIS page's widgets on a working copy of
    // the layout, then Save (persist) or Cancel (discard). Instruments are never
    // created or destroyed here: applyLayout() re-places the SAME instances, so
    // every telemetry pointer Application holds stays valid across edits. When
    // editing is off, the page behaves exactly as before.

    /// Whether Edit Mode is currently on.
    [[nodiscard]] bool isEditing() const { return editing_; }

    /// Enter Edit Mode: snapshot the current layout as the pre-edit baseline and
    /// start a working copy. No-op if already editing.
    void beginEdit();

    /// Save: validate the working copy; on success apply it as the page's layout
    /// and emit layoutCommitted() so the composition root can persist it, then
    /// leave Edit Mode. On failure (invalid working copy) nothing is committed
    /// and Edit Mode stays on. Returns whether the save committed.
    bool saveEdits();

    /// Cancel: discard the working copy, restore the exact pre-edit layout, and
    /// leave Edit Mode. Emits nothing and never persists.
    void cancelEdits();

    /// Move the current selection to the given cell on the working copy, if
    /// valid. No-op when not editing or nothing is selected. Returns success.
    bool moveSelection(layout::DeckRegion region, int row, int column);

    /// Toggle enabled state of the current selection on the working copy, if the
    /// result stays valid. Returns success.
    bool toggleSelectedEnabled();

    /// The currently selected widget (Unknown when none / not editing).
    [[nodiscard]] layout::WidgetId selectedWidget() const { return selected_; }

signals:
    /// Emitted on a successful Save with the page's new layout. The composition
    /// root updates this page's entry in the persisted collection and writes it;
    /// the page itself stays unaware of collections, files, or JSON.
    void layoutCommitted(const layout::DeckLayout& committedLayout);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    /// While editing, intercepts left-press on a registered instrument child so
    /// the page can select it (children otherwise consume their own presses and
    /// the parent's mousePressEvent never fires). Off when not editing.
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    QWidget* buildStatusRegion();
    QWidget* buildNavigationRegion();
    QWidget* buildPrimaryRegion();
    QWidget* buildSecondaryRegion();
    /// Store the concrete instrument for `id` into the matching typed accessor
    /// member, so Application can bind telemetry without the page knowing
    /// telemetry types. Called as the page builds each placement.
    void captureInstrument(layout::WidgetId id, QWidget* widget);

    /// Re-place the EXISTING instrument instances according to `layout`, without
    /// creating or destroying any instrument. Widgets absent from the layout (or
    /// disabled) are hidden and detached from their region layout; present ones
    /// are shown and (re)added at their cell. Pointer identity is preserved, so
    /// telemetry bindings remain valid. Updates layout_ to match.
    void applyLayout(const layout::DeckLayout& layout);

    /// Look up the live widget for an id from the stable instrument map (built
    /// once at construction). Returns nullptr for ids with no instrument.
    [[nodiscard]] QWidget* widgetForId(layout::WidgetId id) const;

    /// The build methods populate this once, mapping each placed WidgetId to its
    /// (permanently owned) instrument widget, so applyLayout can re-place the
    /// same instances instead of asking the factory for new ones.
    void registerInstrument(layout::WidgetId id, QWidget* widget);

    /// Set the current selection to `id`, return keyboard focus to the page (so
    /// arrow keys work immediately), and repaint the selection outline. Shared
    /// by the event filter (clicks on instrument children) and mousePressEvent
    /// (clicks on the page's own surface).
    void selectWidget(layout::WidgetId id);

    /// Build/refresh the EDIT / SAVE / CANCEL controls for the current mode.
    void updateEditControls();

    /// Reposition the selection overlay over the currently selected instrument
    /// (page coords) and raise/show it; hide it when not editing, nothing is
    /// selected, or the selected instrument is hidden.
    void updateSelectionOverlay();

    /// Expand gridHost to fill the Secondary region (editing) so every cell --
    /// including the empty (1,2) -- has real, stable geometry; or restore the
    /// exact normal-mode sizing/alignment (not editing).
    void setEditGridExpanded(bool expanded);

    /// Page-space rectangle of Secondary grid cell (row, column): single source
    /// of truth shared by paintEditGrid (drawing) and gridCellAt (hit-testing).
    [[nodiscard]] QRect cellRectInPage(int row, int column) const;

    /// Hit-test a page-coordinate point against the Secondary grid. True with
    /// row/column filled when inside a cell; false when outside the grid.
    /// occupantOut receives the WidgetId occupying that cell, or Unknown.
    [[nodiscard]] bool gridCellAt(const QPoint& pagePos, int& rowOut,
                                  int& columnOut,
                                  layout::WidgetId& occupantOut) const;

    /// Draw the Secondary grid cells while editing so empty cells are visible,
    /// clickable move targets. Page/edit layer only; never instrument artwork.
    void paintEditGrid(QPainter& painter) const;

    layout::DeckLayout layout_;

    instruments::CpuInstrument* primaryInstrument_ = nullptr;
    instruments::GpuInstrument* gpuInstrument_ = nullptr;
    instruments::MemoryInstrument* memoryInstrument_ = nullptr;
    instruments::CoolingInstrument* coolingInstrument_ = nullptr;
    instruments::StorageInstrument* storageInstrument_ = nullptr;
    instruments::NetworkInstrument* networkInstrument_ = nullptr;
    QList<instruments::CpuInstrument*> shellInstruments_;

    // Stable id -> instrument widget map, built once during construction. The
    // page owns these instruments for its whole lifetime; applyLayout re-places
    // these same instances, so pointers handed to Application never dangle.
    QHash<int, QWidget*> instrumentById_;

    // Region layouts kept so applyLayout can detach/re-add widgets by cell.
    QGridLayout* secondaryGrid_ = nullptr;
    QWidget* gridHost_ = nullptr;
    QVBoxLayout* secondaryOuter_ = nullptr;  // owns gridHost_'s placement
    QWidget* primaryHost_ = nullptr;

    // Page-owned selection overlay, raised above instruments so the selection
    // border is visible. Created once; repositioned per selection.
    class SelectionOverlay* selectionOverlay_ = nullptr;

    // Edit-mode state.
    bool editing_ = false;
    layout::DeckLayout preEditLayout_;   // exact snapshot for Cancel
    layout::DeckLayout workingLayout_;    // mutated during editing
    layout::WidgetId selected_ = layout::WidgetId::Unknown;

    // EDIT / SAVE / CANCEL controls, hosted in the Navigation region.
    QPushButton* editButton_ = nullptr;
    QPushButton* saveButton_ = nullptr;
    QPushButton* cancelButton_ = nullptr;
};

}  // namespace darkspark::deck::pages

#endif  // DARKSPARK_DECK_PAGES_COMMANDDECKPAGE_HPP
