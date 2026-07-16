// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DARKSPARK_DECK_NAVIGATION_PAGEMANAGER_HPP
#define DARKSPARK_DECK_NAVIGATION_PAGEMANAGER_HPP

#include <QWidget>

class QStackedWidget;

namespace darkspark::deck::animations {
class PageTransition;
}
namespace darkspark::deck::pages {
class DeckPage;
}

namespace darkspark::deck::navigation {

class PageIndicator;

/// Owns the set of Deck pages, the active-page state, and navigation between
/// pages. Presents a stacked page area with a page indicator beneath it.
///
/// PageManager is the single source of truth for which page is active. It
/// delegates the visual transition to a PageTransition (animations/), keeping
/// transition logic out of DeckPage. It supports touch swipe (via a filtered
/// gesture on the page area) and keyboard navigation for development.
///
/// Ownership: a QWidget owned by its Qt parent (the DeckWindow). Pages added
/// are reparented into the internal stack. Threading: GUI thread only.
class PageManager : public QWidget {
    Q_OBJECT

public:
    explicit PageManager(QWidget* parent = nullptr);

    /// Add a page. The manager takes ownership via Qt parenting. The first
    /// page added becomes active.
    void addPage(pages::DeckPage* page);

    [[nodiscard]] int pageCount() const;
    [[nodiscard]] int activeIndex() const;

public slots:
    void goToPage(int index);
    void nextPage();
    void previousPage();

signals:
    void activePageChanged(int index);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    void navigateTo(int index);

    QStackedWidget* stack_;
    PageIndicator* indicator_;
    animations::PageTransition* transition_;
    int activeIndex_ = 0;

    // Touch/mouse swipe tracking on the page area.
    bool swipeActive_ = false;
    int swipeStartX_ = 0;
};

}  // namespace darkspark::deck::navigation

#endif  // DARKSPARK_DECK_NAVIGATION_PAGEMANAGER_HPP
