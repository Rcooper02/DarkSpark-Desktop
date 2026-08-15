// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_DECK_PAGES_SELECTIONOVERLAY_HPP
#define DARKSPARK_DECK_PAGES_SELECTIONOVERLAY_HPP

#include <QWidget>

class QPaintEvent;

namespace darkspark::deck::pages {

/// A lightweight, page-owned overlay that draws the Edit Mode selection border
/// ABOVE the instruments. The instruments are child QWidgets that paint over the
/// page surface, so a border drawn in the page's own paintEvent is covered. This
/// overlay is a sibling raised above them, transparent to mouse events, so the
/// border is visible without touching InstrumentRenderer or instrument artwork.
///
/// The owning page positions the overlay over the selected instrument (in page
/// coordinates) and shows/raises it; the overlay only paints a restrained cyan
/// rounded border inside its own rect.
class SelectionOverlay : public QWidget {
    Q_OBJECT

public:
    explicit SelectionOverlay(QWidget* parent);

protected:
    void paintEvent(QPaintEvent* event) override;
};

}  // namespace darkspark::deck::pages

#endif  // DARKSPARK_DECK_PAGES_SELECTIONOVERLAY_HPP
