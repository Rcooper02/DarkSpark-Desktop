// SPDX-License-Identifier: GPL-3.0-or-later
#include "deck/cards/DashboardCard.hpp"

#include "themes/LegacyTheme.hpp"

#include <QFocusEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QStyle>
#include <QVBoxLayout>

#include <initializer_list>

namespace darkspark::deck::cards {

using themes::LegacyTheme;

namespace {

// A small text glyph for the status indicator. Deliberately uses widely
// available glyphs (no icon library, no external fonts) and pairs with the
// status text so meaning is never carried by color alone.
const char* stateGlyph(DashboardCard::State state) {
    switch (state) {
    case DashboardCard::State::Normal:
        return "\u25CF";  // filled circle
    case DashboardCard::State::Loading:
        return "\u25CC";  // dotted circle
    case DashboardCard::State::Empty:
        return "\u25CB";  // hollow circle
    case DashboardCard::State::Unavailable:
        return "\u2014";  // em dash
    case DashboardCard::State::Warning:
        return "\u25B2";  // up-pointing triangle
    case DashboardCard::State::Error:
        return "\u2715";  // cross
    case DashboardCard::State::Disabled:
        return "\u25CB";  // hollow circle
    }
    return "\u25CF";
}

const char* stateStatusText(DashboardCard::State state) {
    switch (state) {
    case DashboardCard::State::Normal:
        return "Placeholder";
    case DashboardCard::State::Loading:
        return "Loading";
    case DashboardCard::State::Empty:
        return "No content";
    case DashboardCard::State::Unavailable:
        return "Unavailable";
    case DashboardCard::State::Warning:
        return "Warning";
    case DashboardCard::State::Error:
        return "Error";
    case DashboardCard::State::Disabled:
        return "Disabled";
    }
    return "Placeholder";
}

const char* stateName(DashboardCard::State state) {
    switch (state) {
    case DashboardCard::State::Normal:
        return "normal";
    case DashboardCard::State::Loading:
        return "loading";
    case DashboardCard::State::Empty:
        return "empty";
    case DashboardCard::State::Unavailable:
        return "unavailable";
    case DashboardCard::State::Warning:
        return "warning";
    case DashboardCard::State::Error:
        return "error";
    case DashboardCard::State::Disabled:
        return "disabled";
    }
    return "normal";
}

const char* accentName(DashboardCard::Accent accent) {
    switch (accent) {
    case DashboardCard::Accent::None:
        return "none";
    case DashboardCard::Accent::Cyan:
        return "cyan";
    case DashboardCard::Accent::Purple:
        return "purple";
    }
    return "none";
}

}  // namespace

DashboardCard::DashboardCard(QString title, QWidget* parent)
    : QFrame(parent), titleLabel_(new QLabel(title, this)),
      subtitleLabel_(new QLabel(QString(), this)),
      statusLabel_(new QLabel(QString(), this)),
      statusDot_(new QLabel(QString(), this)) {
    setObjectName(LegacyTheme::cardObjectName());
    setFrameShape(QFrame::NoFrame);  // border comes from the style sheet
    setFocusPolicy(Qt::StrongFocus);  // keyboard focus for development + a11y

    titleLabel_->setProperty("legacyRole", "cardTitle");
    subtitleLabel_->setProperty("legacyRole", "cardSubtitle");
    statusLabel_->setProperty("legacyRole", "cardStatus");
    statusDot_->setObjectName(LegacyTheme::statusDotObjectName());

    titleLabel_->setWordWrap(true);
    subtitleLabel_->setWordWrap(true);
    statusLabel_->setWordWrap(true);
    subtitleLabel_->setVisible(false);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(LegacyTheme::spaceLg(), LegacyTheme::spaceMd(),
                             LegacyTheme::spaceLg(), LegacyTheme::spaceMd());
    root->setSpacing(LegacyTheme::spaceXs());
    root->addWidget(titleLabel_);
    root->addWidget(subtitleLabel_);
    root->addStretch(1);

    // Status row: indicator dot + status text.
    auto* statusRow = new QHBoxLayout;
    statusRow->setContentsMargins(0, 0, 0, 0);
    statusRow->setSpacing(LegacyTheme::spaceSm());
    statusRow->addWidget(statusDot_);
    statusRow->addWidget(statusLabel_);
    statusRow->addStretch(1);
    root->addLayout(statusRow);

    setMinimumHeight(LegacyTheme::touchTargetMin() * 2);

    applyStateContent();
    refreshVisualState();
}

// --- Content ----------------------------------------------------------------
void DashboardCard::setTitle(const QString& title) { titleLabel_->setText(title); }

void DashboardCard::setSubtitle(const QString& subtitle) {
    subtitleLabel_->setText(subtitle);
    subtitleLabel_->setVisible(!subtitle.isEmpty());
}

void DashboardCard::setStatusText(const QString& status) {
    customStatusText_ = !status.isEmpty();
    statusLabel_->setText(status);
}

QString DashboardCard::title() const { return titleLabel_->text(); }
QString DashboardCard::subtitle() const { return subtitleLabel_->text(); }
QString DashboardCard::statusText() const { return statusLabel_->text(); }

// --- State / accent / size --------------------------------------------------
void DashboardCard::setState(State state) {
    state_ = state;
    setEnabled(state_ != State::Disabled);
    applyStateContent();
    refreshVisualState();
}

DashboardCard::State DashboardCard::state() const { return state_; }

void DashboardCard::setAccent(Accent accent) {
    accent_ = accent;
    setProperty("legacyAccent", QString::fromUtf8(accentName(accent_)));
    refreshVisualState();
}

DashboardCard::Accent DashboardCard::accent() const { return accent_; }

void DashboardCard::setSizeRole(Size size) { size_ = size; }
DashboardCard::Size DashboardCard::sizeRole() const { return size_; }

// --- Internals --------------------------------------------------------------
bool DashboardCard::isInteractive() const { return state_ != State::Disabled; }

void DashboardCard::applyStateContent() {
    statusDot_->setText(QString::fromUtf8(stateGlyph(state_)));
    // The status label carries the state name unless the caller set custom text.
    if (!customStatusText_) {
        statusLabel_->setText(QString::fromUtf8(stateStatusText(state_)));
    }
    // Propagate state onto the status label + title so the style sheet can tint
    // them (color reinforces the glyph + text, never replaces them).
    statusLabel_->setProperty("legacyState", QString::fromUtf8(stateName(state_)));
    titleLabel_->setProperty("legacyState", QString::fromUtf8(stateName(state_)));
    // Re-polish the affected labels.
    for (QLabel* label : {statusLabel_, titleLabel_}) {
        label->style()->unpolish(label);
        label->style()->polish(label);
    }
}

void DashboardCard::refreshVisualState() {
    // Transient interaction visuals take precedence over content state for the
    // card border: pressed, then focused, otherwise the content state.
    QString visual;
    if (pressed_ && isInteractive()) {
        visual = QStringLiteral("pressed");
    } else if (hasFocus() && isInteractive()) {
        visual = QStringLiteral("focused");
    } else {
        visual = QString::fromUtf8(stateName(state_));
    }
    setProperty("legacyState", visual);
    style()->unpolish(this);
    style()->polish(this);
    update();
}

void DashboardCard::mousePressEvent(QMouseEvent* event) {
    if (isInteractive() && event->button() == Qt::LeftButton) {
        pressed_ = true;
        refreshVisualState();
        event->accept();
        return;
    }
    QFrame::mousePressEvent(event);
}

void DashboardCard::mouseReleaseEvent(QMouseEvent* event) {
    if (pressed_ && event->button() == Qt::LeftButton) {
        pressed_ = false;
        refreshVisualState();
        if (isInteractive() && rect().contains(event->pos())) {
            emit activated();
        }
        event->accept();
        return;
    }
    QFrame::mouseReleaseEvent(event);
}

void DashboardCard::keyPressEvent(QKeyEvent* event) {
    if (isInteractive()
        && (event->key() == Qt::Key_Space || event->key() == Qt::Key_Return
            || event->key() == Qt::Key_Enter)) {
        pressed_ = true;
        refreshVisualState();
        event->accept();
        return;
    }
    QFrame::keyPressEvent(event);
}

void DashboardCard::keyReleaseEvent(QKeyEvent* event) {
    if (pressed_
        && (event->key() == Qt::Key_Space || event->key() == Qt::Key_Return
            || event->key() == Qt::Key_Enter)) {
        pressed_ = false;
        refreshVisualState();
        if (isInteractive()) {
            emit activated();
        }
        event->accept();
        return;
    }
    QFrame::keyReleaseEvent(event);
}

void DashboardCard::focusInEvent(QFocusEvent* event) {
    QFrame::focusInEvent(event);
    refreshVisualState();
}

void DashboardCard::focusOutEvent(QFocusEvent* event) {
    pressed_ = false;
    QFrame::focusOutEvent(event);
    refreshVisualState();
}

}  // namespace darkspark::deck::cards
