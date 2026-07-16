// SPDX-License-Identifier: GPL-3.0-or-later
#include "deck/animations/PageTransition.hpp"

#include <QAbstractAnimation>
#include <QEasingCurve>
#include <QParallelAnimationGroup>
#include <QPoint>
#include <QPropertyAnimation>
#include <QStackedWidget>
#include <QWidget>

namespace darkspark::deck::animations {

namespace {
constexpr int kDurationMs = 220;  // motion.standard from docs/design-system.md
}

PageTransition::PageTransition(QStackedWidget* stack, QObject* parent)
    : QObject(parent), stack_(stack) {}

bool PageTransition::isRunning() const { return running_; }

void PageTransition::finishInstantly(int toIndex) {
    if (stack_ != nullptr) {
        stack_->setCurrentIndex(toIndex);
    }
    running_ = false;
    emit settled(toIndex);
}

void PageTransition::transitionTo(int toIndex, Direction direction) {
    if (stack_ == nullptr) {
        return;
    }
    const int count = stack_->count();
    if (toIndex < 0 || toIndex >= count) {
        return;  // out of range: ignore, leave current page as-is
    }

    const int fromIndex = stack_->currentIndex();
    if (running_ || toIndex == fromIndex) {
        // Already animating or no change: switch instantly to stay responsive.
        finishInstantly(toIndex);
        return;
    }

    QWidget* fromPage = stack_->widget(fromIndex);
    QWidget* toPage = stack_->widget(toIndex);
    if (fromPage == nullptr || toPage == nullptr) {
        finishInstantly(toIndex);
        return;
    }

    running_ = true;

    const int width = stack_->width();
    const int sign = (direction == Direction::Forward) ? 1 : -1;

    // Position the incoming page off-screen, then slide both.
    toPage->setGeometry(stack_->rect());
    const QPoint fromStart = fromPage->pos();
    toPage->move(fromStart.x() + sign * width, fromStart.y());
    toPage->show();
    toPage->raise();

    auto* group = new QParallelAnimationGroup(this);

    auto* outAnim = new QPropertyAnimation(fromPage, "pos", group);
    outAnim->setDuration(kDurationMs);
    outAnim->setStartValue(fromStart);
    outAnim->setEndValue(QPoint(fromStart.x() - sign * width, fromStart.y()));
    outAnim->setEasingCurve(QEasingCurve::InOutCubic);

    auto* inAnim = new QPropertyAnimation(toPage, "pos", group);
    inAnim->setDuration(kDurationMs);
    inAnim->setStartValue(QPoint(fromStart.x() + sign * width, fromStart.y()));
    inAnim->setEndValue(fromStart);
    inAnim->setEasingCurve(QEasingCurve::InOutCubic);

    group->addAnimation(outAnim);
    group->addAnimation(inAnim);

    connect(group, &QAbstractAnimation::finished, this, [this, toIndex, fromStart, fromPage]() {
        // Restore the outgoing page's position and let the stack take over.
        stack_->setCurrentIndex(toIndex);
        fromPage->move(fromStart);
        running_ = false;
        emit settled(toIndex);
    });

    group->start(QAbstractAnimation::DeleteWhenStopped);
}

}  // namespace darkspark::deck::animations
