// SPDX-License-Identifier: GPL-3.0-or-later
#include "deck/navigation/PageManager.hpp"

#include "deck/animations/PageTransition.hpp"
#include "deck/navigation/PageIndicator.hpp"
#include "deck/pages/DeckPage.hpp"
#include "themes/LegacyTheme.hpp"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace darkspark::deck::navigation {

using themes::LegacyTheme;

namespace {
// Minimum horizontal travel (device pixels) to count as a swipe rather than a
// tap. Kept generous for touch.
constexpr int kSwipeThreshold = 60;
}  // namespace

PageManager::PageManager(QWidget* parent)
    : QWidget(parent), stack_(new QStackedWidget(this)),
      indicator_(new PageIndicator(this)),
      transition_(new animations::PageTransition(stack_, this)) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(LegacyTheme::spaceMd());
    root->addWidget(stack_, /*stretch=*/1);
    root->addWidget(indicator_, /*stretch=*/0);

    // The stack receives swipe input; watch its events.
    stack_->installEventFilter(this);

    connect(indicator_, &PageIndicator::pageRequested, this, &PageManager::goToPage);
    connect(transition_, &animations::PageTransition::settled, this, [this](int index) {
        activeIndex_ = index;
        indicator_->setState(stack_->count(), activeIndex_);
        emit activePageChanged(index);
    });

    // Keyboard navigation for development.
    setFocusPolicy(Qt::StrongFocus);
}

void PageManager::addPage(pages::DeckPage* page) {
    if (page == nullptr) {
        return;
    }
    stack_->addWidget(page);  // reparents
    if (stack_->count() == 1) {
        activeIndex_ = 0;
        stack_->setCurrentIndex(0);
    }
    indicator_->setState(stack_->count(), activeIndex_);
}

int PageManager::pageCount() const { return stack_->count(); }

int PageManager::activeIndex() const { return activeIndex_; }

void PageManager::navigateTo(int index) {
    if (index < 0 || index >= stack_->count() || index == activeIndex_) {
        return;
    }
    const auto direction = (index > activeIndex_)
                               ? animations::PageTransition::Direction::Forward
                               : animations::PageTransition::Direction::Backward;
    transition_->transitionTo(index, direction);
}

void PageManager::goToPage(int index) { navigateTo(index); }

void PageManager::nextPage() {
    if (activeIndex_ + 1 < stack_->count()) {
        navigateTo(activeIndex_ + 1);
    }
}

void PageManager::previousPage() {
    if (activeIndex_ - 1 >= 0) {
        navigateTo(activeIndex_ - 1);
    }
}

bool PageManager::eventFilter(QObject* watched, QEvent* event) {
    if (watched == stack_) {
        switch (event->type()) {
        case QEvent::MouseButtonPress: {
            auto* me = static_cast<QMouseEvent*>(event);
            swipeActive_ = true;
            swipeStartX_ = me->pos().x();
            return false;  // let children still receive input
        }
        case QEvent::MouseButtonRelease: {
            if (swipeActive_) {
                auto* me = static_cast<QMouseEvent*>(event);
                const int delta = me->pos().x() - swipeStartX_;
                swipeActive_ = false;
                if (delta <= -kSwipeThreshold) {
                    nextPage();
                    return true;
                }
                if (delta >= kSwipeThreshold) {
                    previousPage();
                    return true;
                }
            }
            return false;
        }
        default:
            break;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void PageManager::keyPressEvent(QKeyEvent* event) {
    switch (event->key()) {
    case Qt::Key_Left:
        previousPage();
        event->accept();
        return;
    case Qt::Key_Right:
        nextPage();
        event->accept();
        return;
    default:
        QWidget::keyPressEvent(event);
    }
}

}  // namespace darkspark::deck::navigation
