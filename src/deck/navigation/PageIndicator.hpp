// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_DECK_NAVIGATION_PAGEINDICATOR_HPP
#define DARKSPARK_DECK_NAVIGATION_PAGEINDICATOR_HPP

#include <QWidget>

namespace darkspark::deck::navigation {

/// A row of dots indicating how many pages exist and which is active.
///
/// PageIndicator is presentation only: it reflects state given to it and emits
/// no navigation of its own beyond a tap request. It does not own page state;
/// the PageManager is the source of truth.
///
/// Ownership: a QWidget owned by its Qt parent. Threading: GUI thread only.
class PageIndicator : public QWidget {
    Q_OBJECT

public:
    explicit PageIndicator(QWidget* parent = nullptr);

    /// Set the total number of pages and the active index.
    void setState(int pageCount, int activeIndex);

    [[nodiscard]] int pageCount() const;
    [[nodiscard]] int activeIndex() const;

signals:
    /// Emitted when the user taps a dot, requesting navigation to that page.
    /// The PageManager decides whether/how to honor it.
    void pageRequested(int index);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    [[nodiscard]] QSize sizeHint() const override;

private:
    [[nodiscard]] int dotSpanForHitTest() const;

    int pageCount_ = 0;
    int activeIndex_ = 0;
};

}  // namespace darkspark::deck::navigation

#endif  // DARKSPARK_DECK_NAVIGATION_PAGEINDICATOR_HPP
