// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_DECK_NAVIGATION_PAGEINDICATOR_HPP
#define DARKSPARK_DECK_NAVIGATION_PAGEINDICATOR_HPP

#include <QWidget>

class QVariantAnimation;

namespace darkspark::deck::navigation {

/// A row of markers indicating how many pages exist and which is active.
///
/// The active marker is a larger, brighter, softly-illuminated cyan dot that
/// slides smoothly between positions as the page changes; inactive markers are
/// quiet dots. A dot (not a pill) is used so a navigation element does not adopt
/// the pill shape the visual language reserves for compact status indicators.
/// This is presentation only: PageIndicator reflects state given to it and emits
/// only a tap request. It does not own page state; the PageManager remains the
/// source of truth and its behavior is unchanged.
///
/// Ownership: a QWidget owned by its Qt parent. Threading: GUI thread only.
class PageIndicator : public QWidget {
    Q_OBJECT

public:
    explicit PageIndicator(QWidget* parent = nullptr);

    /// Set the total number of pages and the active index. Animates the active
    /// marker from its current position to the new one.
    void setState(int pageCount, int activeIndex);

    [[nodiscard]] int pageCount() const;
    [[nodiscard]] int activeIndex() const;

signals:
    /// Emitted when the user taps a marker, requesting navigation to that page.
    void pageRequested(int index);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    [[nodiscard]] QSize sizeHint() const override;

private:
    [[nodiscard]] int rowSpan() const;      ///< total width of the marker row
    [[nodiscard]] int slotStride() const;   ///< per-marker horizontal stride
    [[nodiscard]] int rowStartX() const;    ///< left edge of the centered row

    int pageCount_ = 0;
    int activeIndex_ = 0;
    qreal animatedIndex_ = 0.0;  ///< smoothly-animated active position
    QVariantAnimation* slide_;
};

}  // namespace darkspark::deck::navigation

#endif  // DARKSPARK_DECK_NAVIGATION_PAGEINDICATOR_HPP
