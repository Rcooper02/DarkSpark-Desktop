// SPDX-License-Identifier: GPL-3.0-or-later
#include "deck/animations/PageTransition.hpp"

#include "themes/LegacyTheme.hpp"

#include <QAbstractAnimation>
#include <QEasingCurve>
#include <QGraphicsOpacityEffect>
#include <QParallelAnimationGroup>
#include <QPoint>
#include <QPropertyAnimation>
#include <QStackedWidget>
#include <QWidget>

namespace darkspark::deck::animations {

namespace {
// motion.standard, sourced from the centralized theme (docs/VISUAL_LANGUAGE.md).
int durationMs() { return themes::LegacyTheme::motionStandard(); }
// Incoming page starts slightly transparent and fades up — a restrained dip,
// not a full fade (docs/VISUAL_LANGUAGE.md).
constexpr qreal kEnterStartOpacity = 0.6;
}  // namespace

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

    // Slight fade on the incoming page (docs/VISUAL_LANGUAGE.md: "content fades
    // slightly during motion"). A restrained opacity dip, not a full fade, using
    // entry easing (OutCubic). The effect is installed for the duration of the
    // transition and removed on finish so it never lingers or affects idle
    // rendering. Ownership: parented to toPage; explicitly cleared in the
    // finished handler.
    auto* fade = new QGraphicsOpacityEffect(toPage);
    fade->setOpacity(kEnterStartOpacity);
    toPage->setGraphicsEffect(fade);

    auto* group = new QParallelAnimationGroup(this);

    auto* outAnim = new QPropertyAnimation(fromPage, "pos", group);
    outAnim->setDuration(durationMs());
    outAnim->setStartValue(fromStart);
    outAnim->setEndValue(QPoint(fromStart.x() - sign * width, fromStart.y()));
    outAnim->setEasingCurve(QEasingCurve::InOutCubic);  // movement

    auto* inAnim = new QPropertyAnimation(toPage, "pos", group);
    inAnim->setDuration(durationMs());
    inAnim->setStartValue(QPoint(fromStart.x() + sign * width, fromStart.y()));
    inAnim->setEndValue(fromStart);
    inAnim->setEasingCurve(QEasingCurve::InOutCubic);  // movement

    auto* fadeAnim = new QPropertyAnimation(fade, "opacity", group);
    fadeAnim->setDuration(durationMs());
    fadeAnim->setStartValue(kEnterStartOpacity);
    fadeAnim->setEndValue(1.0);
    fadeAnim->setEasingCurve(QEasingCurve::OutCubic);  // entry

    group->addAnimation(outAnim);
    group->addAnimation(inAnim);
    group->addAnimation(fadeAnim);

    connect(group, &QAbstractAnimation::finished, this,
            [this, toIndex, fromStart, fromPage, toPage]() {
                // Restore the outgoing page's position, drop the fade effect so
                // it does not affect idle rendering, and let the stack take over.
                stack_->setCurrentIndex(toIndex);
                fromPage->move(fromStart);
                toPage->setGraphicsEffect(nullptr);
                running_ = false;
                emit settled(toIndex);
            });

    group->start(QAbstractAnimation::DeleteWhenStopped);
}

}  // namespace darkspark::deck::animations
